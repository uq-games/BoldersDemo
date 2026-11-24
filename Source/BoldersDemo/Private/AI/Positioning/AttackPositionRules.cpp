#include "AI/Positioning/AttackPositionRules.h"
#include "Characters/BaseCharacter.h"
#include "Characters/StatueBoss/StatueBossCharacter.h"
#include "Combat/Environment/CombatCollision.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/VolcanoEncounter.h"

bool AttackPositionRules::CanTarget(const ABaseCharacter& Attacker, const AActor* Target)
{
	if (!IsValid(Target) || Target->GetWorld() != Attacker.GetWorld()) return false;
	if (const auto* Boss = Cast<AStatueBossCharacter>(Target); Boss && !Boss->IsAwakened()) return false;
	if (const auto* Character = Cast<ABaseCharacter>(Target))
		return Character->IsAlive() && Attacker.IsHostileTo(Character);
	const auto* Hazard = Cast<AVolcanoHazard>(Target);
	return Hazard && Hazard->IsEncounterActive() && Attacker.GetFaction() == ECharacterFaction::Player;
}

FVector AttackPositionRules::GetAttackPoint(const AActor& Target)
{
	const auto* Hazard = Cast<AVolcanoHazard>(&Target);
	return Hazard ? Hazard->GetAttackPoint() : Target.GetActorLocation();
}

float AttackPositionRules::GetSurfaceReach(const ABaseCharacter& Attacker, float WeaponRange)
{
	return WeaponRange + Attacker.GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float AttackPositionRules::GetReach(const ABaseCharacter& Attacker, const AActor& Target, float WeaponRange)
{
	const auto* Character = Cast<ABaseCharacter>(&Target);
	const auto* Hazard = Cast<AVolcanoHazard>(&Target);
	return GetSurfaceReach(Attacker, WeaponRange)
		+ (Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : (Hazard ? Hazard->GetAttackRadius() : 0.f));
}

bool AttackPositionRules::IsInRange(const ABaseCharacter& Attacker, const AActor& Target, float WeaponRange, const FVector& ActorCenter)
{
	return FVector::DistSquared(ActorCenter, GetAttackPoint(Target)) <= FMath::Square(GetReach(Attacker, Target, WeaponRange));
}

bool AttackPositionRules::CanAttack(const ABaseCharacter& Attacker, const AActor& Target,
	float WeaponRange, const FVector& ActorCenter)
{
	if (!CanTarget(Attacker, &Target) || ActorCenter.ContainsNaN()) return false;
	const FVector AttackPoint = GetAttackPoint(Target);
	if (!IsInRange(Attacker, Target, WeaponRange, ActorCenter)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackSight), false, &Attacker);
	Params.AddIgnoredActor(&Target);
	return !Attacker.GetWorld()->LineTraceTestByObjectType(
		ActorCenter, AttackPoint, BoldersCollision::SolidObjects(), Params);
}

bool AttackPositionRules::CanStandAndAttack(const ABaseCharacter& Attacker, const AActor& Target,
	float WeaponRange, const FVector& NavLocation)
{
	const auto* Capsule = Attacker.GetCapsuleComponent();
	// Navigation points describe the feet; attack checks use the character center.
	const FVector Center = NavLocation + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	if (!CanAttack(Attacker, Target, WeaponRange, Center)
		|| !FApproachWarnings(Attacker).IsPointSafe(NavLocation)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackStandingSpace), false, &Attacker);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 2.f);
	return !Attacker.GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, Shape, Params);
}

AttackPositionRules::FApproachWarnings::FApproachWarnings(const ABaseCharacter& Attacker)
{
	if (Attacker.GetFaction() != ECharacterFaction::Player) return;
	Encounter = AVolcanoEncounter::Find(Attacker.GetWorld());
	if (!Encounter) return;
	for (TActorIterator<AVolcanoHazard> It(Attacker.GetWorld()); It; ++It)
		if (It->HasGroundWarning()) Hazards.Add(*It);
}

bool AttackPositionRules::FApproachWarnings::IsPointSafe(const FVector& GroundPosition) const
{
	if (!Encounter) return true;
	const auto* Boss = Encounter->GetBoss();
	if ((Boss && Boss->IsAttackWarningAt(GroundPosition)) || Encounter->IsGuardianWarningAt(GroundPosition)) return false;
	return !Hazards.ContainsByPredicate([&GroundPosition](const AVolcanoHazard* Hazard)
	{
		return Hazard->ThreatensGround(GroundPosition);
	});
}

bool AttackPositionRules::FApproachWarnings::IsPathSafe(const TArray<FVector>& PathPoints) const
{
	if (!Encounter || PathPoints.IsEmpty()) return true;
	// Someone already inside a warning may leave it. Once clear, the path may not enter again.
	bool bHasReachedSafeGround = IsPointSafe(PathPoints[0]);
	for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
	{
		const FVector& From = PathPoints[Index - 1];
		const FVector& To = PathPoints[Index];
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(From, To) / 35.f));
		for (int32 Sample = 1; Sample <= Samples; ++Sample)
		{
			if (IsPointSafe(FMath::Lerp(From, To, static_cast<double>(Sample) / Samples))) bHasReachedSafeGround = true;
			else if (bHasReachedSafeGround) return false;
		}
	}
	return bHasReachedSafeGround;
}

bool AttackPositionRules::IsPathClearOfTarget(const ABaseCharacter& Attacker, const AActor& Target,
	const TArray<FVector>& PathPoints)
{
	const auto* Character = Cast<ABaseCharacter>(&Target);
	if (!Character) return PathPoints.Num() >= 2;
	// NavMesh does not include these characters as obstacles. Account for both bodies explicitly.
	const float Clearance = Attacker.GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.f;
	FVector TargetPosition = Target.GetActorLocation();
	TargetPosition.Z = 0.f;
	for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
	{
		FVector Start = PathPoints[Index - 1];
		FVector End = PathPoints[Index];
		Start.Z = End.Z = 0.f;
		if (FMath::PointDistToSegmentSquared(TargetPosition, Start, End) < FMath::Square(Clearance))
		{
			return false;
		}
	}
	return PathPoints.Num() >= 2;
}
