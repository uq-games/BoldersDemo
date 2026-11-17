#include "Characters/BaseCharacter.h"
#include "Characters/BaseCharacterAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/FactionRingComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/BoldersAttributeSet.h"
#include "Combat/BoldersDamageEffect.h"
#include "AI/BoldersAIController.h"
#include "AI/AttackPositioningComponent.h"
#include "Net/UnrealNetwork.h"
#include "Combat/CharacterCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	// Small demo arena: relevance must not depend on the stationary server camera.
	bAlwaysRelevant = true;
	AIControllerClass = ABoldersAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->bUseRVOAvoidance = true;
	// Keep local avoidance near the small demo characters; a broad radius causes
	// premature lateral steering while approaching an otherwise clear attack position.
	GetCharacterMovement()->AvoidanceConsiderationRadius = 150.f;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetCanEverAffectNavigation(false);
	Combat = CreateDefaultSubobject<UCharacterCombatComponent>(TEXT("Combat"));
	AttackPositioning = CreateDefaultSubobject<UAttackPositioningComponent>(TEXT("AttackPositioning"));
	FactionRing = CreateDefaultSubobject<UFactionRingComponent>(TEXT("FactionRing"));
	FactionRing->SetupAttachment(GetRootComponent());
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UBoldersAttributeSet>(TEXT("AttributeSet"));
}

void ABaseCharacter::ApplyAnimationBlueprint()
{
	if (AnimationBlueprintClass && GetMesh()->GetAnimClass() != AnimationBlueprintClass)
		GetMesh()->SetAnimInstanceClass(AnimationBlueprintClass);
}

void ABaseCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAnimationBlueprint();
}

UBaseCharacterAnimInstance* ABaseCharacter::GetCharacterAnimation() const
{
	// Mesh owns the instance and can recreate it when its class or mesh changes.
	return Cast<UBaseCharacterAnimInstance>(GetMesh()->GetAnimInstance());
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyAnimationBlueprint();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetMaxHealthAttribute(), InitialMaxHealth);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), GetMaxHealth());
		Combat->GrantBasicAttack();
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute()).RemoveAll(this);
	Super::EndPlay(Reason);
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const { return AbilitySystemComponent; }
float ABaseCharacter::GetHealth() const { return AttributeSet->GetHealth(); }
float ABaseCharacter::GetMaxHealth() const { return AttributeSet->GetMaxHealth(); }
bool ABaseCharacter::IsAlive() const { return GetHealth() > 0.f; }

bool ABaseCharacter::IsHostileTo(const ABaseCharacter* Other) const
{
	return IsValid(Other) && Other != this && Faction != ECharacterFaction::Neutral
		&& Other->Faction != ECharacterFaction::Neutral && Faction != Other->Faction;
}

bool ABaseCharacter::ReceiveDamageFrom(ABaseCharacter* Source, float Damage)
{
	if (!HasAuthority() || !IsAlive() || !IsHostileTo(Source) || !Source->IsAlive()
		|| !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	auto* SourceASC = Source->GetAbilitySystemComponent();
	auto Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);
	auto Spec = SourceASC->MakeOutgoingSpec(UBoldersDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_Data_HealthDelta, -Damage);
	const float Before = GetHealth();
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComponent);
	UE_LOG(LogTemp, Log, TEXT("CombatDamage: %s -> %s health %.1f -> %.1f"), *Source->GetName(), *GetName(), Before, GetHealth());
	return GetHealth() < Before;
}

void ABaseCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue > 0.f && Data.NewValue < Data.OldValue)
	{
		MulticastPlayHitReaction();
		return;
	}
	if (Data.OldValue <= 0.f || Data.NewValue > 0.f) return;
 Combat->CancelAttack();
	if (auto* AI = Cast<ABoldersAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

void ABaseCharacter::MulticastPlayHitReaction_Implementation()
{
	if (IsAlive() && HitReactionMontage) PlayAnimMontage(HitReactionMontage);
}

bool ABaseCharacter::RequestMoveToLocation(const FVector& Destination)
{
	if (!HasAuthority() || !IsAlive() || Destination.ContainsNaN()) return false;
	Combat->CancelAttack();
 ABoldersAIController* AI = Cast<ABoldersAIController>(GetController());
	return AI && AI->RequestCharacterMove(Destination);
}

void ABaseCharacter::SetHoverOutline(bool bEnabled, bool bSelected)
{
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* Part : Meshes)
	{
		Part->SetCustomDepthStencilValue(bSelected ? 201 : 200);
		Part->SetRenderCustomDepth(bEnabled || bSelected);
	}
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ABaseCharacter, PlayerSlot);
 DOREPLIFETIME(ABaseCharacter, Faction);
}
