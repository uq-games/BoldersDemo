#include "Characters/BaseCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BaseCharacterAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/FactionRingComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/BoldersAttributeSet.h"
#include "Combat/BoldersDamageEffect.h"
#include "Combat/AttackPresentation.h"
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
	AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Frozen, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleFrozenChanged);
	if (IsFrozen()) HandleFrozenChanged(TAG_State_Frozen, 1);
	if (HasAuthority())
	{
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetMaxHealthAttribute(), InitialMaxHealth);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), GetMaxHealth());
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetPhysicalResistanceAttribute(), PhysicalResistance);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetFireResistanceAttribute(), FireResistance);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetIceResistanceAttribute(), IceResistance);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetWaterResistanceAttribute(), WaterResistance);
		AbilitySystemComponent->SetNumericAttributeBase(UBoldersAttributeSet::GetLightningResistanceAttribute(), LightningResistance);
		Combat->GrantBasicAttack();
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (LocalWindupVisual.IsValid()) LocalWindupVisual->Destroy();
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute()).RemoveAll(this);
	AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Frozen, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	Super::EndPlay(Reason);
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const { return AbilitySystemComponent; }
float ABaseCharacter::GetHealth() const { return AttributeSet->GetHealth(); }
float ABaseCharacter::GetMaxHealth() const { return AttributeSet->GetMaxHealth(); }
bool ABaseCharacter::IsAlive() const { return GetHealth() > 0.f; }
bool ABaseCharacter::IsFrozen() const { return AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Frozen); }

EElementalAura ABaseCharacter::GetElementalAura() const
{
	const EElementalAura Auras[] = {EElementalAura::Frozen, EElementalAura::Wet, EElementalAura::Chill,
		EElementalAura::Heat, EElementalAura::Charge};
	for (const EElementalAura Aura : Auras)
		if (AbilitySystemComponent->HasMatchingGameplayTag(GetElementalAuraTag(Aura))) return Aura;
	return EElementalAura::None;
}

bool ABaseCharacter::IsHostileTo(const ABaseCharacter* Other) const
{
	return IsValid(Other) && Other != this && Faction != ECharacterFaction::Neutral
		&& Other->Faction != ECharacterFaction::Neutral && Faction != Other->Faction;
}

bool ABaseCharacter::ReceiveDamageFrom(ABaseCharacter* Source, float Damage)
{
	if (!IsValid(Source)) return false;
	const FVector ToSource = (Source->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	return ReceiveDamageAt(Source, Damage, GetActorLocation() + ToSource * GetCapsuleComponent()->GetScaledCapsuleRadius());
}

bool ABaseCharacter::ReceiveDamageAt(ABaseCharacter* Source, float Damage, const FVector& Contact)
{
	if (!ReceiveElementDamage(Source, Damage, EDamageElement::Physical)) return false;
	Source->MulticastAttackVisual(Contact, FRotator::ZeroRotator, true);
	return true;
}

bool ABaseCharacter::ReceiveElementDamage(ABaseCharacter* Source, float Damage, EDamageElement Element)
{
	if (!HasAuthority() || !IsAlive() || !IsHostileTo(Source) || !Source->IsAlive()
		|| !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	auto* SourceASC = Source->GetAbilitySystemComponent();
	auto Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);
	auto Spec = SourceASC->MakeOutgoingSpec(UBoldersDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_Data_HealthDelta, -Damage);
	const FGameplayTag ElementTag = GetDamageElementTag(Element);
	Spec.Data->AddDynamicAssetTag(ElementTag);
	const float Before = GetHealth();
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComponent);
	UE_LOG(LogTemp, Log, TEXT("CombatDamage: %s -> %s health %.1f -> %.1f (%s)"),
		*Source->GetName(), *GetName(), Before, GetHealth(), *ElementTag.ToString());
	return GetHealth() < Before;
}

void ABaseCharacter::ApplyFreezeFrom(ABaseCharacter* Source)
{
	ApplyElementalAura(Source, EElementalAura::Frozen);
}

void ABaseCharacter::ApplyElementalAura(ABaseCharacter* Source, EElementalAura Aura)
{
	if (!HasAuthority() || !IsAlive() || !IsHostileTo(Source)) return;
	const auto Effect = GetElementalAuraEffect(Aura);
	if (!Effect) return;
	const EElementalAura Previous = GetElementalAura();
	auto* SourceASC = Source->GetAbilitySystemComponent();
	const auto Spec = SourceASC->MakeOutgoingSpec(Effect, 1.f, SourceASC->MakeEffectContext());
	if (Spec.IsValid() && SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComponent).IsValid()
		&& Previous != Aura)
	{
		ClearElementalAura(Previous);
	}
}

void ABaseCharacter::ClearElementalAura(EElementalAura Aura)
{
	const FGameplayTag Tag = GetElementalAuraTag(Aura);
	if (!HasAuthority() || !Tag.IsValid()) return;
	AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FGameplayTagContainer(Tag)));
}

void ABaseCharacter::ApplyCombatImpulse(const FVector& VelocityChange)
{
	if (!HasAuthority() || !IsAlive() || VelocityChange.ContainsNaN() || VelocityChange.IsNearlyZero()) return;
	Combat->CancelAttack();
	if (auto* AI = Cast<ABoldersAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->AddImpulse(VelocityChange, true);
	ForceNetUpdate();
}

void ABaseCharacter::HandleFrozenChanged(const FGameplayTag Tag, int32 Count)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (Count > 0 && FrozenVisualMesh && !LocalFrozenVisual)
		{
			LocalFrozenVisual = NewObject<UStaticMeshComponent>(this, TEXT("FrozenCrystalCrown"));
			LocalFrozenVisual->SetupAttachment(GetRootComponent());
			LocalFrozenVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LocalFrozenVisual->SetCanEverAffectNavigation(false);
			LocalFrozenVisual->SetCastShadow(false);
			LocalFrozenVisual->SetStaticMesh(FrozenVisualMesh);
			const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			LocalFrozenVisual->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
			LocalFrozenVisual->SetRelativeScale3D(FVector(Radius / 42.f));
			LocalFrozenVisual->RegisterComponent();
		}
		else if (Count <= 0 && LocalFrozenVisual)
		{
			LocalFrozenVisual->DestroyComponent();
			LocalFrozenVisual = nullptr;
		}
	}
	if (Count <= 0 || !HasAuthority()) return;
	Combat->CancelAttack();
	if (auto* AI = Cast<ABoldersAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
}

void ABaseCharacter::MulticastModuleVisual_Implementation(FName ModuleId, EAttackVisualStage Stage,
	FVector_NetQuantize Location, FVector_NetQuantize End)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const FAttackModule* Module = Combat->FindModule(ModuleId))
		AttackPresentation::Play(*this, *Module, Stage, Location, End);
}

void ABaseCharacter::MulticastElementalReaction_Implementation(EElementalReaction Reaction,
	FVector_NetQuantize Location, FVector_NetQuantize End)
{
	if (GetNetMode() == NM_DedicatedServer || !Combat->AttackLoadout) return;
	if (const auto* Appearance = Combat->AttackLoadout->ReactionAppearance.Get())
		AttackPresentation::PlayReaction(*this, *Appearance, Reaction, Location, End);
}

void ABaseCharacter::MulticastModuleWindup_Implementation(FName ModuleId, float Duration, FVector_NetQuantize Aim)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (LocalWindupVisual.IsValid()) LocalWindupVisual->Destroy();
	LocalWindupVisual.Reset();
	if (Duration <= 0.f) return;
	if (const FAttackModule* Module = Combat->FindModule(ModuleId))
		LocalWindupVisual = AttackPresentation::PlayWindup(*this, *Module, Duration, Aim);
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
	if (!HasAuthority() || !IsAlive() || IsFrozen() || Destination.ContainsNaN()) return false;
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

void ABaseCharacter::MulticastAttackVisual_Implementation(FVector_NetQuantize Location, FRotator Facing, bool bImpact)
{
	UMaterialInterface* Material = bImpact ? ImpactMaterial : SwingMaterial;
	if (!Material || GetNetMode() == NM_DedicatedServer) return;
	static const TSoftObjectPtr<UStaticMesh> Plane(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	UStaticMesh* PlaneMesh = Plane.LoadSynchronous();
	if (!PlaneMesh) return;
	FRotator Rotation(12.f, Facing.Yaw, -18.f);
	if (bImpact)
	{
		if (const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
			{
			const FVector ViewDirection = (Camera->GetCameraLocation() - Location).GetSafeNormal();
			Rotation = FRotationMatrix::MakeFromZ(ViewDirection).Rotator();
			Location += ViewDirection * 14.f;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Flash = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!Flash) return;
	auto* Surface = Flash->GetStaticMeshComponent();
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->SetStaticMesh(PlaneMesh);
	Flash->SetActorScale3D(FVector(bImpact ? 1.45f : 1.9f));
	auto* Instance = Surface->CreateDynamicMaterialInstance(0, Material);
	if (Instance) Instance->SetScalarParameterValue(TEXT("StartTime"), GetWorld()->GetTimeSeconds());
	Flash->SetLifeSpan(bImpact ? 0.36f : 0.34f);
}
