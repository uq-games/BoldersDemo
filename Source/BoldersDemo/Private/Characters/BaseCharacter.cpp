#include "Characters/BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "AI/BoldersAIController.h"
#include "AI/Positioning/AttackPositioningComponent.h"
#include "Camera/CameraTypes.h"
#include "Characters/Animation/BaseCharacterAnimInstance.h"
#include "Characters/BoldersCombatMovementComponent.h"
#include "Characters/Presentation/FactionRingComponent.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Effects/BoldersDamageEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/BoldersPlayerController.h"

ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoldersCombatMovementComponent>(CharacterMovementComponentName))
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
	GetMesh()->SetReceivesDecals(false);
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

void ABaseCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAnimationBlueprint();
}

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!GetWorld()->IsGameWorld() || FMath::IsNearlyEqual(CombatModelScale, 1.f)) return;

	USkeletalMeshComponent& VisualMesh = *GetMesh();
	const double FloorZ = -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	FVector Offset = VisualMesh.GetRelativeLocation();
	Offset.Z = FloorZ + (Offset.Z - FloorZ) * CombatModelScale;
	VisualMesh.SetRelativeLocation(Offset);
	VisualMesh.SetRelativeScale3D(VisualMesh.GetRelativeScale3D() * CombatModelScale);
	CacheInitialMeshOffset(Offset, VisualMesh.GetRelativeRotation());
}

FVector ABaseCharacter::GetOverheadAnchor(float Clearance) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return GetActorLocation() + FVector(0, 0, HalfHeight * (2.f * CombatModelScale - 1.f) + Clearance);
}

void ABaseCharacter::ApplyAnimationBlueprint()
{
	if (AnimationBlueprintClass && GetMesh()->GetAnimClass() != AnimationBlueprintClass)
		GetMesh()->SetAnimInstanceClass(AnimationBlueprintClass);
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

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABaseCharacter, PlayerSlot);
	DOREPLIFETIME(ABaseCharacter, Faction);
}

UBaseCharacterAnimInstance* ABaseCharacter::GetCharacterAnimation() const
{
	// Mesh owns the instance and can recreate it when its class or mesh changes.
	return Cast<UBaseCharacterAnimInstance>(GetMesh()->GetAnimInstance());
}

bool ABaseCharacter::IsHostileTo(const ABaseCharacter* Other) const
{
	return IsValid(Other) && Other != this && Faction != ECharacterFaction::Neutral
		&& Other->Faction != ECharacterFaction::Neutral && Faction != Other->Faction;
}

bool ABaseCharacter::HasDirectCombatInput() const
{
	const auto* PC = Cast<ABoldersPlayerController>(GetController());
	return PC && PC->UsesDirectCombatControls();
}

void ABaseCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (!HasDirectCombatInput())
	{
		Super::CalcCamera(DeltaTime, OutResult);
		return;
	}

	const FRotator View = ABoldersPlayerController::GetFixedCombatViewRotation();
	const FVector GroundFocus = GetActorLocation() + FVector(0, 0, 38.f);
	OutResult.Rotation = View;
	OutResult.Location = GroundFocus - View.Vector() * 2110.f;
	OutResult.FOV = 56.f;
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == this)
		AbilitySystemComponent->RefreshAbilityActorInfo();
}

void ABaseCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == this)
		AbilitySystemComponent->RefreshAbilityActorInfo();
}

bool ABaseCharacter::RequestMoveToLocation(const FVector& Destination)
{
	if (!HasAuthority() || !IsAlive() || IsFrozen() || Destination.ContainsNaN()) return false;
	Combat->CancelAttack();
	ABoldersAIController* AI = Cast<ABoldersAIController>(GetController());
	return AI && AI->RequestCharacterMove(Destination);
}
