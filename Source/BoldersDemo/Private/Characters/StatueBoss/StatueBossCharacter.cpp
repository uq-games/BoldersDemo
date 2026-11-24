#include "Characters/StatueBoss/StatueBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Characters/Presentation/FactionRingComponent.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Characters/StatueBoss/StatueBossAnimInstance.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Presentation/AttackPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogStatueBoss, Log, All);

namespace
{
	constexpr float BodyRadius = 118.f;
	constexpr float BodyHalfHeight = 150.f;
	constexpr float AxeReach = 435.f;
	constexpr float SweepReach = 490.f;
	constexpr float LeapMinReach = 570.f;
	constexpr float LeapMaxReach = 885.f;
	constexpr float MaxAttackHeightDifference = 160.f;
	constexpr float WalkReferenceSpeed = 254.f;

	bool IsAttack(EStatueBossAction Action)
	{
		return Action == EStatueBossAction::AxeSlam || Action == EStatueBossAction::Cleave
			|| Action == EStatueBossAction::LeapingSlam;
	}

	bool IsTransition(EStatueBossAction Action)
	{
		return Action == EStatueBossAction::Awakening || Action == EStatueBossAction::Enraging
			|| Action == EStatueBossAction::Staggered;
	}

	template<typename T>
	T* LoadDefaultAsset(const TCHAR* Path)
	{
		return ConstructorHelpers::FObjectFinder<T>(Path).Object;
	}

	template<typename T>
	T* LoadOptionalAsset(const TCHAR* Path)
	{
		return ConstructorHelpers::FObjectFinderOptional<T>(Path, LOAD_Quiet | LOAD_NoWarn).Get();
	}

	FVector Feet(const ABaseCharacter& Character)
	{
		return Character.GetActorLocation()
			- FVector::UpVector * Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
}

AStatueBossCharacter::AStatueBossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Faction = ECharacterFaction::Neutral;
	InitialMaxHealth = 420.f;
	FireResistance = .20f;
	LightningResistance = -.12f;
	GetCapsuleComponent()->InitCapsuleSize(BodyRadius, BodyHalfHeight);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCharacterMovement()->MaxWalkSpeed = 237.f;
	GetCharacterMovement()->MaxStepHeight = 55.f;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 160.f, 0.f);
	GetCharacterMovement()->bUseRVOAvoidance = false;
	GetCharacterMovement()->bCanWalkOffLedges = false;

	// Keep the movement capsule low for melee range and navmesh queries, with a separate selection
	// volume that makes the head and shoulders clickable from the isometric camera.
	UpperBodySelection = CreateDefaultSubobject<UCapsuleComponent>(TEXT("UpperBodySelection"));
	UpperBodySelection->SetupAttachment(GetRootComponent());
	UpperBodySelection->SetRelativeLocation(FVector(0.f, 0.f, 295.f));
	UpperBodySelection->InitCapsuleSize(124.f, 183.f);
	UpperBodySelection->SetCollisionObjectType(ECC_Pawn);
	UpperBodySelection->SetCollisionResponseToAllChannels(ECR_Ignore);
	UpperBodySelection->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UpperBodySelection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UpperBodySelection->SetGenerateOverlapEvents(false);
	UpperBodySelection->SetCanEverAffectNavigation(false);

	GetMesh()->SetSkeletalMeshAsset(LoadDefaultAsset<USkeletalMesh>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/SK_StatueBoss_Mixamo.SK_StatueBoss_Mixamo")));
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -BodyHalfHeight));
	GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	IdleAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Idle.A_StatueBoss_Mixamo_Idle"));
	WalkAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Walk.A_StatueBoss_Mixamo_Walk"));
	AxeSlamAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Chop.A_StatueBoss_Mixamo_Chop"));
	LeapAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_LeapTravel.A_StatueBoss_Mixamo_LeapTravel"));
	CleaveAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_SpinLow.A_StatueBoss_Mixamo_SpinLow"));
	AwakeningAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Awakening.A_StatueBoss_Mixamo_Awakening"));
	EnrageAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Enrage.A_StatueBoss_Mixamo_Enrage"));
	StaggerAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Hit.A_StatueBoss_Mixamo_Hit"));
	DefeatAnimation = LoadDefaultAsset<UAnimSequence>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/Animations/A_StatueBoss_Mixamo_Defeat.A_StatueBoss_Mixamo_Defeat"));
	Axe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Axe"));
	Axe->SetupAttachment(GetMesh(), TEXT("AxeGrip"));
	Axe->SetReceivesDecals(false);
	Axe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Axe->SetCanEverAffectNavigation(false);
	Axe->SetStaticMesh(LoadDefaultAsset<UStaticMesh>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe/SM_StatueBoss_MixamoAxe.SM_StatueBoss_MixamoAxe")));
	FrozenVisualMesh = LoadDefaultAsset<UStaticMesh>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/SM_ConceptFrozenCrystals.SM_ConceptFrozenCrystals"));

	FactionRing->Radius = 171.f;
	WarningOutline = CreateDefaultSubobject<UDecalComponent>(TEXT("WarningOutline"));
	WarningOutline->SetupAttachment(GetRootComponent());
	WarningOutline->SetAbsolute(true, true, true);
	WarningOutline->SetDecalMaterial(LoadDefaultAsset<UMaterialInterface>(CombatGuideStyle::DangerMaterialPath));
	WarningOutline->FadeScreenSize = 0.f;
	WarningOutline->SetSortOrder(CombatGuideStyle::DangerSort);
	WarningOutline->SetVisibility(false);

	FurnaceGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("FurnaceGlow"));
	FurnaceGlow->SetupAttachment(GetRootComponent());
	FurnaceGlow->SetMobility(EComponentMobility::Movable);
	FurnaceGlow->SetRelativeLocation(FVector(105.f, 0.f, 210.f));
	FurnaceGlow->SetUseInverseSquaredFalloff(false);
	FurnaceGlow->SetAttenuationRadius(565.f);
	FurnaceGlow->SetCastShadows(false);
	FurnaceGlow->SetVisibility(false);

	StoneImpactMaterial = LoadDefaultAsset<UMaterialInterface>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/M_Concept_HeavyContact.M_Concept_HeavyContact"));
	FireImpactMaterial = LoadDefaultAsset<UMaterialInterface>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/M_FireballBurst.M_FireballBurst"));
	FireImpactRing = LoadDefaultAsset<UMaterialInterface>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/M_Concept_FireRing.M_Concept_FireRing"));
	StoneImpactParticles = LoadDefaultAsset<UNiagaraSystem>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/NS_Impact_Steel.NS_Impact_Steel"));
	FireImpactParticles = LoadDefaultAsset<UNiagaraSystem>(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/NS_Impact_Flame.NS_Impact_Flame"));

	FootfallSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Footfall.SW_StatueBoss_Footfall"));
	AxeImpactSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Axe.SW_StatueBoss_Axe"));
	StompImpactSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Stomp.SW_StatueBoss_Stomp"));
	AwakeningSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Awakening.SW_StatueBoss_Awakening"));
	EnrageSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Enrage.SW_StatueBoss_Enrage"));
	DefeatSound = LoadOptionalAsset<USoundBase>(
		TEXT("/Game/BoldersDemo/Assets/Characters/StatueBoss/Audio/SW_StatueBoss_Defeat.SW_StatueBoss_Defeat"));
	CueAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("StatueCueFalloff"));
	CueAttenuation->Attenuation.bSpatialize = true;
	CueAttenuation->Attenuation.bAttenuate = true;
	CueAttenuation->Attenuation.AttenuationShapeExtents = FVector(1450.f, 0.f, 0.f);
	CueAttenuation->Attenuation.FalloffDistance = 2550.f;
}

void AStatueBossCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		GetMesh()->OverrideAnimationData(GetActionAnimation(EStatueBossAction::Dormant), false, false);
	}
}

void AStatueBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
	if (GetNetMode() != NM_DedicatedServer)
	{
		GetMesh()->SetAnimInstanceClass(UStatueBossAnimInstance::StaticClass());
		if (WarningOutline->GetDecalMaterial()) OutlineMaterial = WarningOutline->CreateDynamicMaterialInstance();
	}
	if (HasAuthority())
	{
		HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleBossHealthChanged);
	}
	OnRep_ActionState();
	OnRep_Enraged();
}

void AStatueBossCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HealthChangedHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBoldersAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	}
	Super::EndPlay(Reason);
}

void AStatueBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStatueBossCharacter, ActionState);
	DOREPLIFETIME(AStatueBossCharacter, bEnraged);
}

double AStatueBossCharacter::GetCombatTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AStatueBossCharacter::ActivateBoss()
{
	if (!HasAuthority() || IsAwakened() || !IsAlive()) return;
	Faction = ECharacterFaction::Enemy;
	BeginTransition(EStatueBossAction::Awakening, GetCombatTime());
	UE_LOG(LogStatueBoss, Log, TEXT("%s awakened at %.0f HP"), *GetName(), GetHealth());
}

void AStatueBossCharacter::StopForEncounterEnd()
{
	if (!HasAuthority() || !IsAlive() || bEncounterStopped) return;
	bEncounterStopped = true;
	bEnrageQueued = false;
	PendingAttackDamage = 0.f;
	StopPursuit();
	if (IsAwakened()) SetAction({EStatueBossAction::Idle, GetCombatTime()});
}

void AStatueBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetCombatTime();
	if (HasAuthority() && IsAlive() && !bEncounterStopped) UpdateAuthority(Now);
	if (GetNetMode() != NM_DedicatedServer) UpdatePresentation(Now, DeltaSeconds);
}

void AStatueBossCharacter::HandleBossHealthChanged(const FOnAttributeChangeData& Change)
{
	if (bEncounterStopped || ActionState.Action == EStatueBossAction::Defeated || !IsAwakened()) return;
	if (Change.NewValue <= 0.f)
	{
		BeginTransition(EStatueBossAction::Defeated, GetCombatTime());
		UE_LOG(LogStatueBoss, Log, TEXT("%s defeated"), *GetName());
		return;
	}
	if (!bEnraged && Change.NewValue <= GetMaxHealth() * .45f)
	{
		bEnrageQueued = true;
		return;
	}
	const double Now = GetCombatTime();
	// A heavy hit during the idle gap can reel the knight. Committed windups need a real interrupt.
	if (ActionState.Action != EStatueBossAction::Idle || Change.OldValue - Change.NewValue < 26.f
		|| Now - LastHitFlinchAt < 5.5 || Now + .25 >= NextAttackAt) return;
	LastHitFlinchAt = Now;
	BeginTransition(EStatueBossAction::Staggered, Now);
}

void AStatueBossCharacter::UpdateAuthority(double Now)
{
	if (!IsAwakened()) return;
	const EStatueBossAction Action = ActionState.Action;
	const double Elapsed = Now - ActionState.StartedAt;
	if (IsFrozen())
	{
		if (IsAttack(Action) && !ActionState.bReleased)
		{
			BeginTransition(EStatueBossAction::Staggered, Now);
			UE_LOG(LogStatueBoss, Log, TEXT("%s interrupted by Freeze"), *GetName());
		}
		else if (Action == EStatueBossAction::Walk)
		{
			StopPursuit();
			SetAction({EStatueBossAction::Idle, Now});
		}
		NextAttackAt = FMath::Max(NextAttackAt, Now + .4);
		return;
	}
	if (IsAttack(Action))
	{
		if (Now >= NextDecisionAt && !ActionState.bReleased)
		{
			NextDecisionAt = Now + .18;
			if (!FindOpponent())
			{
				SetAction({EStatueBossAction::Idle, Now});
				return;
			}
		}
		const float Turn = FMath::SmoothStep(0.f, 1.f, static_cast<float>(Elapsed / .27));
		const float FacingYaw = ActionState.Heading.Rotation().Yaw;
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(AttackStartYaw, FacingYaw);
		const float Yaw = AttackStartYaw + DeltaYaw * Turn;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Yaw)) > .25f)
			SetActorRotation(FRotator(0.f, Yaw, 0.f));
		if (Action == EStatueBossAction::LeapingSlam && !ActionState.bReleased && !AdvanceLeap(Elapsed))
		{
			BeginTransition(EStatueBossAction::Staggered, Now);
			return;
		}
		if (!ActionState.bReleased && Elapsed >= ActionState.HitAfter)
		{
			ActionState.bReleased = true;
			ResolveAttack();
			ForceNetUpdate();
		}
		// Finish the committed hit and leave one counterattack window before the phase change.
		const float RecoveryEnd = bEnrageQueued && ActionState.bReleased
			? FMath::Min(ActionState.Duration, ActionState.HitAfter + .78f) : ActionState.Duration;
		if (Elapsed < RecoveryEnd) return;
		SetAction({EStatueBossAction::Idle, Now});
	}
	else if (IsTransition(Action))
	{
		if (Elapsed < ActionState.Duration) return;
		SetAction({EStatueBossAction::Idle, Now});
	}
	if (bEnrageQueued)
	{
		bEnrageQueued = false;
		bEnraged = true;
		GetCharacterMovement()->MaxWalkSpeed = 286.f;
		BeginTransition(EStatueBossAction::Enraging, Now);
		UE_LOG(LogStatueBoss, Log, TEXT("%s enraged at %.0f HP"), *GetName(), GetHealth());
		return;
	}
	if (Now < NextDecisionAt) return;
	NextDecisionAt = Now + .18;
	DecideAction(Now);
}

ABaseCharacter* AStatueBossCharacter::FindOpponent() const
{
	ABaseCharacter* Closest = nullptr;
	double BestDistance = FMath::Square(EngagementRadius);
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		ABaseCharacter* Candidate = *It;
		if (!IsHostileTo(Candidate) || !Candidate->IsAlive()
			|| FVector::DistSquared2D(HomeLocation, Candidate->GetActorLocation()) > FMath::Square(EngagementRadius)) continue;
		const double Distance = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < BestDistance)
		{
			Closest = Candidate;
			BestDistance = Distance;
		}
	}
	return Closest;
}

void AStatueBossCharacter::DecideAction(double Now)
{
	ABaseCharacter* Opponent = FindOpponent();
	auto* AI = Cast<AAIController>(GetController());
	const bool bCanPursue = Opponent && AI
		&& FVector::DistSquared2D(HomeLocation, Opponent->GetActorLocation()) < FMath::Square(ChaseRadius);
	if (!Opponent)
	{
		StopPursuit();
		if (ActionState.Action != EStatueBossAction::Idle) SetAction({EStatueBossAction::Idle, Now});
		return;
	}

	const double Distance = FVector::Dist2D(GetActorLocation(), Opponent->GetActorLocation());
	const bool bClearPath = FMath::Abs(Feet(*this).Z - Feet(*Opponent).Z) <= MaxAttackHeightDifference
		&& AttackResolution::HasClearPath(*this, *Opponent, GetActorLocation());
	const bool bReady = Now >= NextAttackAt;
	if (bReady && bClearPath)
	{
		if (Distance <= SweepReach)
		{
			const bool bSweep = Distance > AxeReach || AttacksStarted % 3 == 1;
			BeginAttack(bSweep ? EStatueBossAction::Cleave : EStatueBossAction::AxeSlam, *Opponent, Now);
			return;
		}
		if (Distance >= LeapMinReach && Distance <= LeapMaxReach && TryBeginLeap(*Opponent, Now)) return;
	}
	if (!bCanPursue || (Distance <= AxeReach * .67f && bClearPath))
	{
		StopPursuit();
		if (ActionState.Action != EStatueBossAction::Idle) SetAction({EStatueBossAction::Idle, Now});
		return;
	}
	if (Now < NextPursuitAt || (PursuitTarget.Get() == Opponent && AI->GetMoveStatus() == EPathFollowingStatus::Moving)) return;
	NextPursuitAt = Now + .65;
	const auto Result = AI->MoveToActor(Opponent, 248.f, false, true, true, nullptr, false);
	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		PursuitTarget = Opponent;
		if (ActionState.Action != EStatueBossAction::Walk)
			SetAction({EStatueBossAction::Walk, Now});
		return;
	}
	PursuitTarget.Reset();
	if (ActionState.Action != EStatueBossAction::Idle)
	{
		SetAction({EStatueBossAction::Idle, Now});
	}
}

void AStatueBossCharacter::StopPursuit()
{
	if (auto* AI = Cast<AAIController>(GetController()); AI && AI->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		AI->StopMovement();
	}
	PursuitTarget.Reset();
	GetCharacterMovement()->StopMovementImmediately();
}

bool AStatueBossCharacter::TryBeginLeap(const ABaseCharacter& Opponent, double Now)
{
	const FVector Start = Feet(*this);
	const FVector ToOpponent = (Feet(Opponent) - Start).GetSafeNormal2D();
	const double Travel = FMath::Clamp(FVector::Dist2D(Start, Feet(Opponent)) - 500.0, 108.0, 300.0);
	const FVector Destination = Start + ToOpponent * Travel;
	const auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Landing;
	FVector BlockedAt;
	if (!Nav || !Nav->ProjectPointToNavigation(Destination, Landing, FVector(28.f, 28.f, 95.f))
		|| FVector::Dist2D(Destination, Landing.Location) > 22.f
		|| FMath::Abs(Landing.Location.Z - Start.Z) > 62.f
		|| UNavigationSystemV1::NavigationRaycast(this, Start, Landing.Location, BlockedAt, nullptr, GetController()))
		return false;

	const auto* Capsule = GetCapsuleComponent();
	const FVector BottomClearance(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 25.f);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(StatueLeapClearance), false, this);
	if (GetWorld()->SweepTestByObjectType(Start + BottomClearance, Landing.Location + BottomClearance,
		FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * .94f, Capsule->GetScaledCapsuleHalfHeight() - 13.f), Params))
		return false;

	LeapStart = GetActorLocation();
	LeapEnd = Landing.Location + FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
	BeginAttack(EStatueBossAction::LeapingSlam, Opponent, Now);
	return true;
}

bool AStatueBossCharacter::AdvanceLeap(double Elapsed)
{
	// The source FBX has this 108 cm travel removed from the hips.
	const float Time = GetAnimationSampleTime(Elapsed);
	const float Progress = .48f * FMath::SmoothStep(.45f, 1.60f, Time)
		+ .52f * FMath::SmoothStep(2.08f, 3.52f, Time);
	FVector Destination = FMath::Lerp(LeapStart, LeapEnd, Progress);
	auto* Motion = GetCharacterMovement();
	const double HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector EstimatedFeet = Destination - FVector::UpVector * HalfHeight;
	FHitResult Floor;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(StatueLeapFloor), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Floor, EstimatedFeet + FVector(0, 0, 66),
		EstimatedFeet - FVector(0, 0, 84), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		&& Motion->IsWalkable(Floor))
		Destination.Z = Floor.ImpactPoint.Z + HalfHeight + 2.4;

	const FVector Delta = Destination - GetActorLocation();
	FHitResult Blocker;
	Motion->SafeMoveUpdatedComponent(Delta, GetActorQuat(), true, Blocker);
	if (!Blocker.IsValidBlockingHit()) return true;
	if (Motion->IsWalkable(Blocker))
	{
		const FVector Slide = FVector::VectorPlaneProject(Delta, Blocker.Normal) * (1.f - Blocker.Time);
		Motion->SafeMoveUpdatedComponent(Slide, GetActorQuat(), true, Blocker);
		if (!Blocker.IsValidBlockingHit() || Motion->IsWalkable(Blocker)) return true;
	}
	return FVector::DistSquared2D(Destination, GetActorLocation()) <= FMath::Square(22.f)
		&& (Progress < .99f || Cast<ABaseCharacter>(Blocker.GetActor()));
}

void AStatueBossCharacter::BeginAttack(EStatueBossAction Attack, const ABaseCharacter& Opponent, double Now)
{
	const bool bAxe = Attack == EStatueBossAction::AxeSlam;
	const bool bCleave = Attack == EStatueBossAction::Cleave;
	const bool bLeap = Attack == EStatueBossAction::LeapingSlam;
	StopPursuit();
	FVector Direction = (Opponent.GetActorLocation() - GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, GetActorForwardVector());
	if (bAxe) Direction = Direction.RotateAngleAxis(43.f, FVector::UpVector);
	const FVector Right(-Direction.Y, Direction.X, 0.f);
	AttackStartYaw = GetActorRotation().Yaw;

	FStatueBossActionState AttackState{Attack, Now};
	AttackState.Heading = Direction;
	AttackState.PlayRate = bLeap ? (bEnraged ? 1.62f : 1.43f)
		: (bCleave ? (bEnraged ? 1.38f : 1.20f) : (bEnraged ? 1.52f : 1.30f));
	// These offsets are the sampled blade/boot contact poses in the corresponding source files.
	AttackState.HitAfter = (bAxe ? 71.f : (bCleave ? 62.f : 107.f)) / (30.f * AttackState.PlayRate);
	AttackState.Radius = bAxe ? 239.f : (bCleave ? 324.f : 286.f);
	AttackState.HalfWidth = bAxe ? 164.f : (bCleave ? 335.f : 238.f);
	FVector Center = bLeap ? LeapEnd - FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : Feet(*this);
	Center += Direction * (bAxe ? 205.f : (bCleave ? 173.f : 434.f));
	Center += Right * (bAxe ? -215.f : (bLeap ? -35.f : 0.f));
	if (bLeap) Center.Z = FMath::Lerp(Center.Z, Feet(Opponent).Z, .7);
	FHitResult Ground;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(StatueBossGround), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Ground, Center + FVector(0.f, 0.f, 75.f),
		Center - FVector(0.f, 0.f, 145.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Center = Ground.ImpactPoint;
		AttackState.GroundNormal = Ground.ImpactNormal;
	}
	AttackState.GroundCenter = Center;
	const UAnimSequence* Animation = GetActionAnimation(Attack);
	AttackState.Duration = Animation ? Animation->GetPlayLength() / AttackState.PlayRate : AttackState.HitAfter + 1.35f;
	NextAttackAt = Now + AttackState.Duration + (bEnraged ? .48 : .72);
	PendingAttackDamage = (bAxe ? 27.f : (bLeap ? 33.f : 22.f)) * (bEnraged ? 1.13f : 1.f);
	++AttacksStarted;
	SetAction(AttackState);
	UE_LOG(LogStatueBoss, Log, TEXT("%s began %s at %s, length %.0f, width %.0f, windup %.2fs"), *GetName(),
		*UEnum::GetValueAsString(Attack), *Center.ToCompactString(), AttackState.Radius,
		AttackState.HalfWidth, AttackState.HitAfter);
}

void AStatueBossCharacter::BeginTransition(EStatueBossAction Next, double Now)
{
	StopPursuit();
	FStatueBossActionState State{Next, Now};
	State.PlayRate = Next == EStatueBossAction::Enraging ? 1.30f : 1.f;
	const UAnimSequence* Sequence = GetActionAnimation(Next);
	State.Duration = Sequence && Sequence != IdleAnimation.Get() ? Sequence->GetPlayLength() / State.PlayRate : 1.2f;
	if (Next != EStatueBossAction::Defeated)
		NextAttackAt = FMath::Max(NextAttackAt, Now + State.Duration + .20);
	SetAction(State);
}

bool AStatueBossCharacter::IsPointInWarning(const FVector& WorldPoint) const
{
	if (!IsAttack(ActionState.Action) || WorldPoint.ContainsNaN() || ActionState.Radius <= 0.f || ActionState.HalfWidth <= 0.f)
		return false;
	const FVector Offset = WorldPoint - FVector(ActionState.GroundCenter);
	const FVector Forward = ActionState.Heading;
	const FVector Right(-Forward.Y, Forward.X, 0.f);
	const double Along = FVector::DotProduct(Offset, Forward) / ActionState.Radius;
	const double Across = FVector::DotProduct(Offset, Right) / ActionState.HalfWidth;
	return Along * Along + Across * Across <= 1.0;
}

bool AStatueBossCharacter::IsRecovering() const
{
	return (IsAttack(ActionState.Action) && ActionState.bReleased)
		|| ActionState.Action == EStatueBossAction::Staggered;
}

bool AStatueBossCharacter::IsAttackWarningAt(const FVector& GroundPosition) const
{
	return !ActionState.bReleased && IsPointInWarning(GroundPosition) && GetCombatTime() < GetWarningEndsAt()
		&& FMath::Abs(GroundPosition.Z - ActionState.GroundCenter.Z) <= MaxAttackHeightDifference;
}

void AStatueBossCharacter::ResolveAttack()
{
	const EStatueBossAction Attack = ActionState.Action;
	const FVector Center = ActionState.GroundCenter;
	// A tall volume gathers candidates. The visible ellipse determines damage in horizontal space.
	const FVector TraceOrigin = Center + FVector(0.f, 0.f, 42.f);
	FAttackModule HitModule;
	HitModule.Element = EDamageElement::Physical;
	FElementalHitBatch Batch;
	int32 Hits = 0;
	const float TestRadius = FMath::Max(ActionState.Radius, ActionState.HalfWidth) + MaxAttackHeightDifference;
	for (ABaseCharacter* Target : AttackResolution::FindTargets(*this, TraceOrigin, TestRadius))
	{
		const FVector TargetFeet = Feet(*Target);
		if (!IsPointInWarning(TargetFeet)
			|| FMath::Abs(TargetFeet.Z - Center.Z) > MaxAttackHeightDifference
			|| !AttackResolution::HasClearPath(*this, *Target, TraceOrigin)) continue;
		if (!AttackResolution::ApplyHit(*this, *Target, HitModule, PendingAttackDamage, &Batch)) continue;
		++Hits;
		if (Attack == EStatueBossAction::LeapingSlam && Target->IsAlive())
		{
			const FVector Away = (TargetFeet - Center).GetSafeNormal2D(UE_SMALL_NUMBER, GetActorForwardVector());
			Target->ApplyCombatImpulse(Away * 168.f + FVector(0.f, 0.f, 65.f));
		}
	}
	UE_LOG(LogStatueBoss, Log, TEXT("%s resolved %s; hit %d"), *GetName(), *UEnum::GetValueAsString(Attack), Hits);
}

void AStatueBossCharacter::SetAction(FStatueBossActionState NewAction)
{
	NewAction.Revision = ActionState.Revision + 1;
	ActionState = NewAction;
	OnRep_ActionState();
	ForceNetUpdate();
}

UAnimSequence* AStatueBossCharacter::GetActionAnimation(EStatueBossAction Action) const
{
	switch (Action)
	{
	case EStatueBossAction::Walk: return WalkAnimation;
	case EStatueBossAction::Dormant:
	case EStatueBossAction::Awakening: return AwakeningAnimation ? AwakeningAnimation.Get() : IdleAnimation.Get();
	case EStatueBossAction::Enraging: return EnrageAnimation ? EnrageAnimation.Get() : IdleAnimation.Get();
	case EStatueBossAction::Staggered: return StaggerAnimation ? StaggerAnimation.Get() : IdleAnimation.Get();
	case EStatueBossAction::Defeated: return DefeatAnimation ? DefeatAnimation.Get() : IdleAnimation.Get();
	case EStatueBossAction::Cleave: return CleaveAnimation;
	case EStatueBossAction::AxeSlam: return AxeSlamAnimation;
	case EStatueBossAction::LeapingSlam: return LeapAnimation;
	default: return IdleAnimation;
	}
}

void AStatueBossCharacter::OnRep_ActionState()
{
	if (!HasActorBegunPlay()) return;
	const EStatueBossAction Action = ActionState.Action;
	const bool bDefeated = Action == EStatueBossAction::Defeated;
	const bool bTargetable = IsAwakened() && !bDefeated;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, bTargetable ? ECR_Block : ECR_Ignore);
	UpperBodySelection->SetCollisionEnabled(bTargetable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (bDefeated)
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorTickEnabled(IsAwakened() && (!bDefeated || GetNetMode() != NM_DedicatedServer));
	if (GetNetMode() == NM_DedicatedServer) return;

	const double Now = GetCombatTime();
	if (PresentedRevision != ActionState.Revision)
	{
		PresentedRevision = ActionState.Revision;
		bImpactPresented = false;
		bTransitionPresented = false;
		bDefeatAxePresented = false;
		if (Action == EStatueBossAction::Walk)
		{
			WalkSpeed = static_cast<float>(GetVelocity().Size2D());
			WalkTime = WalkSpeed / WalkReferenceSpeed * static_cast<float>(FMath::Max(0.0, Now - ActionState.StartedAt));
		}
		if (auto* Pose = Cast<UStatueBossAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			const float BlendTime = bDefeated ? .20f : (IsTransition(Action) ? .18f : .14f);
			Pose->StartPose(GetActionAnimation(Action), GetAnimationSampleTime(Now - ActionState.StartedAt), BlendTime);
		}
		if (IsAttack(Action))
		{
			if (!OutlineMaterial) OutlineMaterial = WarningOutline->CreateDynamicMaterialInstance();
			CombatGuideStyle::PlaceGroundDecal(*WarningOutline, ActionState.GroundCenter, ActionState.Heading,
				ActionState.Radius, ActionState.HalfWidth, MaxAttackHeightDifference);
			if (OutlineMaterial)
			{
				const bool bLeap = Action == EStatueBossAction::LeapingSlam;
				const bool bCleave = Action == EStatueBossAction::Cleave;
				const FLinearColor Tint = bLeap ? FLinearColor(1.f, .62f, .16f)
					: bCleave ? FLinearColor(1.f, .33f, .10f) : FLinearColor(1.f, .16f, .085f);
				OutlineMaterial->SetVectorParameterValue(TEXT("RingColor"), Tint);
				OutlineMaterial->SetScalarParameterValue(TEXT("MarkStyle"), bLeap ? 5.f : bCleave ? 4.f : 3.f);
				OutlineMaterial->SetScalarParameterValue(TEXT("StartTime"), GetWorld()->GetTimeSeconds()
					- FMath::Max(0.0, Now - ActionState.StartedAt));
				OutlineMaterial->SetScalarParameterValue(TEXT("Lifetime"), ActionState.HitAfter);
			}
		}
	}
	OnRep_Enraged();
	UpdatePresentation(Now, 0.f);
}

void AStatueBossCharacter::OnRep_Enraged()
{
	if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer) return;
	FurnaceGlow->SetVisibility(IsAwakened() && ActionState.Action != EStatueBossAction::Defeated);
	FurnaceGlow->SetLightColor(bEnraged ? FLinearColor(1.f, .12f, .018f) : FLinearColor(1.f, .54f, .18f));
	FurnaceGlow->SetIntensity(bEnraged ? 7.f : 2.5f);
}

float AStatueBossCharacter::GetAnimationSampleTime(double Elapsed) const
{
	const EStatueBossAction Action = ActionState.Action;
	const UAnimSequence* Sequence = GetActionAnimation(Action);
	if (!Sequence || Action == EStatueBossAction::Dormant) return 0.f;
	const float Length = FMath::Max(Sequence->GetPlayLength(), UE_SMALL_NUMBER);
	const float SampleTime = static_cast<float>(FMath::Max(0.0, Elapsed)) * ActionState.PlayRate;
	if (Action == EStatueBossAction::Idle) return FMath::Fmod(SampleTime, Length);
	if (Action == EStatueBossAction::Walk) return FMath::Fmod(WalkTime, Length);
	return FMath::Min(SampleTime, Length);
}

void AStatueBossCharacter::UpdateWalk(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f || !WalkAnimation) return;
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	WalkSpeed = FMath::FInterpTo(WalkSpeed, Speed, DeltaSeconds, 10.f);
	if (WalkSpeed < 5.f) return;
	const float Length = FMath::Max(.01f, WalkAnimation->GetPlayLength());
	const float Previous = FMath::Fmod(WalkTime, Length);
	const float Next = Previous + WalkSpeed / WalkReferenceSpeed * DeltaSeconds;
	// Boot touchdown frames sampled from the 30 fps Mixamo walk and checked against the armor.
	const FName FeetBones[] = {TEXT("LeftFoot"), TEXT("RightFoot")};
	const float Contacts[] = {14.f / 30.f, 47.f / 30.f};
	if (Speed > 45.f)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Contacts); ++Index)
		{
			const float Beat = Contacts[Index] + (Previous >= Contacts[Index] ? Length : 0.f);
			if (Next < Beat) continue;
			const FVector Contact = GetMesh()->GetSocketLocation(FeetBones[Index]) - FVector(0.f, 0.f, 46.f);
			PlayWorldCue(FootfallSound, Contact, .66f);
		}
	}
	WalkTime = FMath::Fmod(Next, Length);
}

void AStatueBossCharacter::UpdatePresentation(double Now, float DeltaSeconds)
{
	const EStatueBossAction Action = ActionState.Action;
	const double Elapsed = FMath::Max(0.0, Now - ActionState.StartedAt);
	if (Action == EStatueBossAction::Walk) UpdateWalk(DeltaSeconds);
	if (auto* Pose = Cast<UStatueBossAnimInstance>(GetMesh()->GetAnimInstance()))
		Pose->SetSampleTime(GetAnimationSampleTime(Elapsed));
	const bool bAttacking = IsAttack(Action);
	const bool bWindup = bAttacking && Elapsed < ActionState.HitAfter;
	const double AfterHit = Elapsed - ActionState.HitAfter;
	const bool bShowImpactRing = bAttacking && ActionState.bReleased && AfterHit >= 0.0 && AfterHit < .22;
	WarningOutline->SetVisibility(bWindup || bShowImpactRing);
	if ((bWindup || bShowImpactRing) && OutlineMaterial)
		OutlineMaterial->SetScalarParameterValue(TEXT("Contact"), bShowImpactRing ? 1.f : 0.f);
	if (bAttacking && ActionState.bReleased && !bImpactPresented && AfterHit >= 0.0)
	{
		bImpactPresented = true;
		if (AfterHit < .32) PlayImpactPresentation();
	}
	if (IsTransition(Action) || Action == EStatueBossAction::Defeated)
	{
		PlayTransitionPresentation(Elapsed);
		if (Action == EStatueBossAction::Defeated && Elapsed >= ActionState.Duration + .10) SetActorTickEnabled(false);
	}
}

void AStatueBossCharacter::PlayTransitionPresentation(double Elapsed)
{
	if (bTransitionPresented) return;
	const EStatueBossAction Action = ActionState.Action;
	const bool bDefeat = Action == EStatueBossAction::Defeated;
	const bool bEnrage = Action == EStatueBossAction::Enraging;
	if (bDefeat && !bDefeatAxePresented && Elapsed >= 2.30)
	{
		bDefeatAxePresented = true;
		if (Elapsed < 2.58) PlayWorldCue(AxeImpactSound, Axe->GetComponentLocation(), .58f);
	}
	const double CueAt = bDefeat ? 3.12 : (bEnrage ? .69 : .55);
	if (Action == EStatueBossAction::Staggered || Elapsed < CueAt) return;
	bTransitionPresented = true;
	if (Elapsed - CueAt > .28) return;
	const FVector Point = bDefeat
		? GetMesh()->GetSocketLocation(TEXT("Spine2")) - FVector(0.f, 0.f, 38.f)
		: Feet(*this) + FVector(0.f, 0.f, 26.f);
	USoundBase* Sound = bDefeat ? DefeatSound.Get() : (bEnrage ? EnrageSound.Get() : AwakeningSound.Get());
	PlayWorldCue(Sound, Point);
	if (auto* Particles = bEnrage ? FireImpactParticles.Get() : StoneImpactParticles.Get())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Particles, Point,
			FRotator(90.f, 0.f, 0.f), FVector(bDefeat ? 2.5f : 1.8f));
	}
}

void AStatueBossCharacter::PlayWorldCue(USoundBase* Sound, const FVector& Origin, float Volume) const
{
	if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, Origin, Volume, 1.f, 0.f, CueAttenuation);
}

void AStatueBossCharacter::PlayImpactPresentation()
{
	const bool bLeap = ActionState.Action == EStatueBossAction::LeapingSlam;
	FAttackModule Impact;
	Impact.Pattern = EAttackPattern::Blast;
	Impact.Radius = ActionState.Radius;
	Impact.Impact = bEnraged && FireImpactMaterial ? FireImpactMaterial : StoneImpactMaterial;
	Impact.ImpactParticles = bEnraged && FireImpactParticles ? FireImpactParticles : StoneImpactParticles;
	const FVector Center = FVector(ActionState.GroundCenter) + FVector(0.f, 0.f, 15.f);
	AttackPresentation::Play(*this, Impact, EAttackVisualStage::Impact, Center, Center + FVector(ActionState.Heading));
	if (bEnraged && bLeap && FireImpactRing)
	{
		Impact.Impact = FireImpactRing;
		Impact.ImpactParticles = nullptr;
		AttackPresentation::Play(*this, Impact, EAttackVisualStage::Impact, Center, Center + FVector(ActionState.Heading));
	}
	PlayWorldCue(bLeap ? StompImpactSound.Get() : AxeImpactSound.Get(), Center, bLeap ? 1.f : .88f);
}
