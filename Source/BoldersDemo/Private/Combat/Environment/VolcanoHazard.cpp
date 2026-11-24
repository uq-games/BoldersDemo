#include "Combat/Environment/VolcanoHazard.h"

#include "Combat/Environment/CombatCollision.h"
#include "Combat/Environment/CoolingBasinShape.h"

#include "AbilitySystemComponent.h"
#include "Characters/BaseCharacter.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/Presentation/AttackPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

namespace
{
	constexpr float EruptionWarning = 1.65f;
	constexpr float EruptionDuration = 2.15f;
	constexpr float EruptionRecovery = 8.5f;
	constexpr float BarrelFuse = 1.35f;
	constexpr float OrbWindup = .8f;
	constexpr float PoolChargeDuration = 2.8f;

	double GetClock(const UWorld& World)
	{
		const auto* GameState = World.GetGameState();
		return GameState ? GameState->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
}

AVolcanoHazard::AVolcanoHazard()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 1.f / 30.f;

	auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("HazardRoot"));
	SetRootComponent(Root);

	const auto AddTarget = [this, Root](const FName Name)
	{
		auto* Target = CreateDefaultSubobject<USphereComponent>(Name);
		Target->SetupAttachment(Root);
		Target->InitSphereRadius(40.f);
		Target->SetCollisionObjectType(BoldersCollision::Interactable);
		Target->SetCollisionResponseToAllChannels(ECR_Ignore);
		Target->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Target->SetGenerateOverlapEvents(false);
		Target->SetCanEverAffectNavigation(false);
		return Target;
	};
	AttackTarget = AddTarget(TEXT("AttackTarget"));
	AttackTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Ground-hugging wave projectiles can hit the orb's stone pedestal.
	OrbPedestalTarget = AddTarget(TEXT("OrbPedestalTarget"));
	OrbPedestalTarget->SetRelativeLocation(FVector(0, 0, 38));
	OrbPedestalTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("EnvironmentAbilitySystem"));

	const auto AddMesh = [this, Root](const FName Name)
	{
		auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetGenerateOverlapEvents(false);
		return Mesh;
	};
	Body = AddMesh(TEXT("Body"));
	Core = AddMesh(TEXT("Core"));
	Core->SetCastShadow(false);

	PhaseEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PhaseEffect"));
	PhaseEffect->SetupAttachment(Root);
	PhaseEffect->SetAutoActivate(false);
	PhaseEffect->SetCanEverAffectNavigation(false);

	WaterSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterSurface"));
	WaterSurface->SetupAttachment(Root);
	WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterSurface->SetCanEverAffectNavigation(false);
	WaterSurface->SetCastShadow(false);
	WaterSurface->SetRelativeLocation(FVector(0, 0, 8));

	GroundMark = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GroundMark"));
	GroundMark->SetupAttachment(Root);
	GroundMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundMark->SetCanEverAffectNavigation(false);
	GroundMark->SetCastShadow(false);
	GroundMark->SetRelativeLocation(FVector(0, 0, 5));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetCastShadows(false);
	Glow->SetAttenuationRadius(330.f);
	Glow->SetIntensity(0.f);

	SoundAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("HazardSoundAttenuation"));
	SoundAttenuation->Attenuation.bSpatialize = true;
	SoundAttenuation->Attenuation.bAttenuate = true;
	SoundAttenuation->Attenuation.AttenuationShapeExtents = FVector(950.f, 0.f, 0.f);
	SoundAttenuation->Attenuation.FalloffDistance = 2400.f;
	LoadPresentationAssets();
}

void AVolcanoHazard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigurePresentation();
	UpdatePresentation();
}

void AVolcanoHazard::BeginPlay()
{
	Super::BeginPlay();
	bPhasePresentationReady = true;
	ConfigurePresentation();
	UpdatePresentation();
	if (GetNetMode() != NM_DedicatedServer)
		SetActorTickEnabled(Kind == EVolcanoHazardKind::LightningOrb);

	if (!HasAuthority()) return;
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (Kind == EVolcanoHazardKind::Eruption)
		ChangePhase(EVolcanoHazardState::Cooling, 4.f + CycleOffset);
	else if (Kind == EVolcanoHazardKind::Puddle)
		ChangePhase(EVolcanoHazardState::Ready, 0.f);
}

void AVolcanoHazard::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

void AVolcanoHazard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AVolcanoHazard, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AVolcanoHazard, Radius, COND_InitialOnly);
	DOREPLIFETIME(AVolcanoHazard, Phase);
}

UAbilitySystemComponent* AVolcanoHazard::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

float AVolcanoHazard::GetEffectRadius() const
{
	if (Radius > 0.f) return FMath::Max(Radius, 12.f);
	switch (Kind)
	{
	case EVolcanoHazardKind::Puddle: return 185.f;
	case EVolcanoHazardKind::ExplosiveBarrel: return 340.f;
	case EVolcanoHazardKind::LightningOrb: return 375.f;
	default: return 245.f;
	}
}

FVector AVolcanoHazard::GetAttackPoint() const
{
	return AttackTarget->GetComponentLocation();
}

float AVolcanoHazard::GetAttackRadius() const
{
	return AttackTarget->GetScaledSphereRadius();
}

bool AVolcanoHazard::CanReceiveAttack() const
{
	if (Phase.bEncounterEnded) return false;
	switch (Kind)
	{
	case EVolcanoHazardKind::ExplosiveBarrel:
	case EVolcanoHazardKind::LightningOrb: return Phase.State == EVolcanoHazardState::Ready;
	case EVolcanoHazardKind::Puddle: return Phase.State != EVolcanoHazardState::Steaming;
	default: return true;
	}
}

bool AVolcanoHazard::CanReactTo(EDamageElement Element) const
{
	if (!CanReceiveAttack()) return false;
	using State = EVolcanoHazardState;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		return Element == EDamageElement::Water || Element == EDamageElement::Ice
			|| Phase.State == State::Cooling || Phase.State == State::Ready;
	case EVolcanoHazardKind::Puddle:
		if (Element == EDamageElement::Fire) return true;
		if (Element == EDamageElement::Ice) return Phase.State != State::Frozen;
		return Element == EDamageElement::Lightning && Phase.State != State::Frozen && Phase.State != State::Active;
	default:
		return true;
	}
}

void AVolcanoHazard::DeactivateForEncounterEnd()
{
	if (!HasAuthority() || Phase.bEncounterEnded) return;
	Phase.bEncounterEnded = true;
	EVolcanoHazardState Settled = EVolcanoHazardState::Cooling;
	if (Kind == EVolcanoHazardKind::Puddle) Settled = EVolcanoHazardState::Ready;
	else if (Kind == EVolcanoHazardKind::ExplosiveBarrel)
		Settled = Phase.State == EVolcanoHazardState::Active || Phase.State == EVolcanoHazardState::Spent
			? EVolcanoHazardState::Spent : EVolcanoHazardState::Ready;
	ChangePhase(Settled, 0.f);
	GetWorldTimerManager().ClearTimer(PulseTimer);
}

bool AVolcanoHazard::HasGroundWarning() const
{
	if (Phase.bEncounterEnded) return false;
	if (Kind == EVolcanoHazardKind::Puddle) return Phase.State == EVolcanoHazardState::Active;
	return Phase.State == EVolcanoHazardState::Warning
		|| (Kind == EVolcanoHazardKind::Eruption && Phase.State == EVolcanoHazardState::Active);
}

bool AVolcanoHazard::ThreatensGround(const FVector& Position) const
{
	if (!HasGroundWarning()) return false;
	const FVector Offset = Position - GetActorLocation();
	if (FMath::Abs(Offset.Z) > 130.f) return false;
	if (Kind == EVolcanoHazardKind::Puddle)
		return CoolingBasinShape::Contains(GetActorTransform().InverseTransformVectorNoScale(Offset), GetEffectRadius());
	return Offset.SizeSquared2D() <= FMath::Square(GetEffectRadius());
}

bool AVolcanoHazard::IntersectsAreaImpact(const FVector& Center, float AreaRadius) const
{
	const FVector Offset = Center - GetActorLocation();
	if (FMath::Abs(Offset.Z) > 155.f || AreaRadius <= 0.f) return false;
	if (Kind != EVolcanoHazardKind::Puddle)
		return Offset.SizeSquared2D() <= FMath::Square(AreaRadius + GetAttackRadius());

	using namespace CoolingBasinShape;
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Offset);
	const double HalfLength = GetEffectRadius();
	if (Contains(Local, HalfLength)) return true;
	const auto Boundary = Outline(HalfLength, HalfLength * HalfWidthRatio, HalfLength * CornerRatio);
	const FVector Point(Local.X, Local.Y, 0);
	for (int32 Index = 0; Index < CornerCount; ++Index)
	{
		const FVector A(Boundary[Index], 0);
		const FVector B(Boundary[(Index + 1) % CornerCount], 0);
		if (FMath::PointDistToSegmentSquared(Point, A, B) <= FMath::Square(AreaRadius)) return true;
	}
	return false;
}

EDamageElement AVolcanoHazard::GetBladeDipContact(const FVector& Feet, FVector& Contact) const
{
	if (Kind != EVolcanoHazardKind::Puddle || !CanReceiveAttack()) return EDamageElement::Physical;

	using namespace CoolingBasinShape;
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Feet - GetActorLocation());
	const FVector Point(Local.X, Local.Y, 0);
	const double HalfLength = GetEffectRadius();
	FVector Closest = Point;
	if (!Contains(Point, HalfLength))
	{
		const auto Boundary = Outline(HalfLength, HalfLength * HalfWidthRatio, HalfLength * CornerRatio);
		double Best = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < CornerCount; ++Index)
		{
			const FVector A(Boundary[Index], 0);
			const FVector B(Boundary[(Index + 1) % CornerCount], 0);
			const FVector Candidate = FMath::ClosestPointOnSegment(Point, A, B);
			const double Distance = FVector::DistSquared(Point, Candidate);
			if (Distance >= Best) continue;
			Best = Distance;
			Closest = Candidate;
		}
	}
	Contact = GetActorLocation() + GetActorTransform().TransformVectorNoScale(Closest + FVector(0, 0, 10));
	if (Phase.State == EVolcanoHazardState::Frozen) return EDamageElement::Ice;
	if (Phase.State == EVolcanoHazardState::Active) return EDamageElement::Lightning;
	return EDamageElement::Water;
}

bool AVolcanoHazard::ReceiveAttack(ABaseCharacter* Source, EDamageElement Element)
{
	if (!HasAuthority() || !IsValid(Source) || !Source->IsAlive() || Source->GetFaction() == ECharacterFaction::Neutral
		|| !CanReactTo(Element)) return false;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		if (Element == EDamageElement::Water || Element == EDamageElement::Ice)
		{
			ChangePhase(EVolcanoHazardState::Cooling, 7.f);
			return true;
		}
		if (Phase.State == EVolcanoHazardState::Cooling || Phase.State == EVolcanoHazardState::Ready)
		{
			ChangePhase(EVolcanoHazardState::Warning, EruptionWarning);
			return true;
		}
		return false;
	case EVolcanoHazardKind::Puddle:
		return ApplyToPool(Element);
	case EVolcanoHazardKind::ExplosiveBarrel:
		ChangePhase(EVolcanoHazardState::Warning, BarrelFuse);
		return true;
	case EVolcanoHazardKind::LightningOrb:
		ChangePhase(EVolcanoHazardState::Warning, OrbWindup);
		return true;
	}
	return false;
}

bool AVolcanoHazard::ApplyToPool(EDamageElement Element)
{
	if (!CanReactTo(Element)) return false;
	switch (Element)
	{
	case EDamageElement::Lightning:
		ChangePhase(EVolcanoHazardState::Active, PoolChargeDuration);
		return true;
	case EDamageElement::Ice:
		ChangePhase(EVolcanoHazardState::Frozen, 4.f);
		return true;
	case EDamageElement::Fire:
		ChangePhase(EVolcanoHazardState::Steaming, 4.f);
		return true;
	default: return false;
	}
}

void AVolcanoHazard::ChangePhase(EVolcanoHazardState State, float Seconds)
{
	auto& Timers = GetWorldTimerManager();
	Timers.ClearTimer(PhaseTimer);
	Timers.ClearTimer(PulseTimer);
	Phase.State = State;
	Phase.StartedAt = static_cast<float>(GetClock(*GetWorld()));
	Phase.Duration = Seconds;
	++Phase.Revision;
	ForceNetUpdate();
	OnRep_Phase();

	if (Seconds > 0.f) Timers.SetTimer(PhaseTimer, this, &ThisClass::AdvancePhase, Seconds);
	const bool bPeriodicPool = Kind == EVolcanoHazardKind::Puddle &&
		(State == EVolcanoHazardState::Ready || State == EVolcanoHazardState::Active || State == EVolcanoHazardState::Frozen);
	const bool bErupting = Kind == EVolcanoHazardKind::Eruption && State == EVolcanoHazardState::Active;
	if (State == EVolcanoHazardState::Active || bPeriodicPool) Pulse();
	if (bErupting || bPeriodicPool)
	{
		const float Interval = bErupting ? 1.f : (State == EVolcanoHazardState::Active ? 1.25f : .45f);
		Timers.SetTimer(PulseTimer, this, &ThisClass::Pulse, Interval, true);
	}
}

void AVolcanoHazard::AdvancePhase()
{
	using State = EVolcanoHazardState;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		if (Phase.State == State::Warning) ChangePhase(State::Active, EruptionDuration);
		else if (Phase.State == State::Active) ChangePhase(State::Cooling, EruptionRecovery);
		else ChangePhase(State::Warning, EruptionWarning);
		break;
	case EVolcanoHazardKind::ExplosiveBarrel:
		if (Phase.State == State::Warning) ChangePhase(State::Active, .65f);
		else ChangePhase(State::Spent, 0.f);
		break;
	case EVolcanoHazardKind::LightningOrb:
		if (Phase.State == State::Warning) ChangePhase(State::Active, .65f);
		else if (Phase.State == State::Active) ChangePhase(State::Cooling, 9.f);
		else ChangePhase(State::Ready, 0.f);
		break;
	case EVolcanoHazardKind::Puddle:
		ChangePhase(State::Ready, 0.f);
		break;
	}
}

void AVolcanoHazard::Pulse()
{
	if (Phase.bEncounterEnded) return;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		AffectCharacters(EDamageElement::Fire, 15.f);
		break;
	case EVolcanoHazardKind::ExplosiveBarrel:
		AffectCharacters(EDamageElement::Fire, 78.f);
		TriggerNeighbors();
		break;
	case EVolcanoHazardKind::LightningOrb:
		AffectCharacters(EDamageElement::Lightning, 46.f);
		TriggerNeighbors();
		break;
	case EVolcanoHazardKind::Puddle:
		if (Phase.State == EVolcanoHazardState::Active)
			AffectCharacters(EDamageElement::Lightning, 15.f);
		else if (Phase.State == EVolcanoHazardState::Frozen)
			AffectCharacters(EDamageElement::Ice, 0.f);
		else
			AffectCharacters(EDamageElement::Water, 0.f);
		break;
	}
}

void AVolcanoHazard::AffectCharacters(EDamageElement Element, float Damage)
{
	const FVector Origin = GetActorLocation();
	const float EffectRadius = GetEffectRadius();
	constexpr float MaxGroundDelta = 130.f;
	TArray<FOverlapResult> Results;
	GetWorld()->OverlapMultiByObjectType(Results, Origin + FVector(0, 0, 130), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(FVector(EffectRadius, EffectRadius, 270)),
		FCollisionQueryParams(SCENE_QUERY_STAT(VolcanoHazard), false, this));

	TSet<ABaseCharacter*> Visited;
	for (const auto& Result : Results)
	{
		auto* Target = Cast<ABaseCharacter>(Result.GetActor());
		if (!IsValid(Target) || !Target->IsAlive() || Target->GetFaction() == ECharacterFaction::Neutral
			|| Visited.Contains(Target)) continue;
		Visited.Add(Target);
		const FVector Offset = Target->GetActorLocation() - Origin;
		const auto* Capsule = Target->GetCapsuleComponent();
		const double Distance = Offset.Size2D();
		const bool bInFootprint = Kind == EVolcanoHazardKind::Puddle
			? CoolingBasinShape::Contains(GetActorTransform().InverseTransformVectorNoScale(Offset), EffectRadius)
			: Distance <= EffectRadius;
		if (!bInFootprint || FMath::Abs(Offset.Z - Capsule->GetScaledCapsuleHalfHeight()) > MaxGroundDelta
			|| !IsUnobstructed(Origin + Offset)) continue;

		const double Fraction = Distance / EffectRadius;
		const bool bBlast = Kind == EVolcanoHazardKind::ExplosiveBarrel;
		const float EffectiveDamage = bBlast ? Damage * FMath::Lerp(1.f, .66f, static_cast<float>(Fraction)) : Damage;
		const EElementalReaction Reaction =
			AttackResolution::ApplyEnvironmentHit(*this, *AbilitySystem, *Target, Element, EffectiveDamage);

		if (bBlast && Capsule->GetScaledCapsuleRadius() < 95.f)
			Target->ApplyCombatImpulse((Offset.GetSafeNormal2D() + FVector(0, 0, .35)) * (350.0 * (1.0 - Fraction)));

		const FVector Contact = Target->GetActorLocation();
		if (Kind == EVolcanoHazardKind::LightningOrb)
		{
			MulticastReaction(EElementalReaction::Conduct, Contact, GetAttackPoint());
			if (Reaction == EElementalReaction::Conduct) continue;
		}
		if (Reaction != EElementalReaction::None) MulticastReaction(Reaction, Contact, Contact);
	}
}

bool AVolcanoHazard::IsUnobstructed(const FVector& WorldLocation) const
{
	return !GetWorld()->LineTraceTestByObjectType(GetActorLocation() + FVector(0, 0, 46), WorldLocation,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(VolcanoObstacle), false, this));
}

void AVolcanoHazard::TriggerNeighbors()
{
	using KindType = EVolcanoHazardKind;
	using State = EVolcanoHazardState;
	for (TActorIterator<AVolcanoHazard> It(GetWorld()); It; ++It)
	{
		AVolcanoHazard& Other = **It;
		if (&Other == this) continue;

		const bool bChainBarrel = Kind == KindType::ExplosiveBarrel && Other.Kind == KindType::ExplosiveBarrel
			&& Other.Phase.State == State::Ready;
		const bool bChargePool = Kind == KindType::LightningOrb && Other.Kind == KindType::Puddle;
		if (!bChainBarrel && !bChargePool) continue;

		const float MaxReach = GetEffectRadius() + (bChargePool ? Other.GetEffectRadius() * .65f : 0.f);
		const FVector Target = Other.GetActorLocation() + FVector(0, 0, 35);
		if (FVector::DistSquared2D(GetActorLocation(), Target) > FMath::Square(MaxReach)
			|| FMath::Abs(Target.Z - GetActorLocation().Z) > 165.f || !IsUnobstructed(Target)) continue;
		if (bChainBarrel) Other.ChangePhase(State::Warning, BarrelFuse);
		else if (Other.ApplyToPool(EDamageElement::Lightning))
			MulticastReaction(EElementalReaction::Conduct, Target, GetAttackPoint());
	}
}

float AVolcanoHazard::GetPhaseAge() const
{
	return static_cast<float>(FMath::Max(0.0, GetClock(*GetWorld()) - Phase.StartedAt));
}

void AVolcanoHazard::OnRep_Configuration()
{
	ConfigurePresentation();
	UpdatePresentation();
	if (HasActorBegunPlay() && GetNetMode() != NM_DedicatedServer)
		SetActorTickEnabled(Kind == EVolcanoHazardKind::LightningOrb);
}

void AVolcanoHazard::OnRep_Phase()
{
	UpdatePresentation();
	if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer) return;
	const float Age = GetPhaseAge();
	if (Age >= .55f) return;
	PlayPhaseSound(Age);
}

void AVolcanoHazard::MulticastReaction_Implementation(EElementalReaction Reaction, FVector_NetQuantize Location,
	FVector_NetQuantize End)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (Kind == EVolcanoHazardKind::LightningOrb && Reaction == EElementalReaction::Conduct &&
		PlayConductLink(End, Location)) return;
	if (Reactions)
		AttackPresentation::PlayReaction(*this, *Reactions, Reaction, Location, End);
}
