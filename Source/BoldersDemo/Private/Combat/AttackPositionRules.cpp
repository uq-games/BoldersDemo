#include "Combat/AttackPositionRules.h"
#include "Characters/BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

float AttackPositionRules::GetReach(const ABaseCharacter& Attacker, const ABaseCharacter& Target, float WeaponRange)
{
	return WeaponRange + Attacker.GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Target.GetCapsuleComponent()->GetScaledCapsuleRadius();
}

bool AttackPositionRules::CanAttack(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
	float WeaponRange, const FVector& ActorCenter)
{
	if (!Target.IsAlive() || ActorCenter.ContainsNaN()) return false;
	const float Reach = GetReach(Attacker, Target, WeaponRange);
	if (FVector::DistSquared(ActorCenter, Target.GetActorLocation()) > FMath::Square(Reach)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackSight), false, &Attacker);
	Params.AddIgnoredActor(&Target);
	return !Attacker.GetWorld()->LineTraceTestByChannel(
		ActorCenter, Target.GetActorLocation(), ECC_Visibility, Params);
}

bool AttackPositionRules::CanStandAndAttack(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
	float WeaponRange, const FVector& NavLocation)
{
	const auto* Capsule = Attacker.GetCapsuleComponent();
	// Navigation points describe the feet; attack checks use the character center.
	const FVector Center = NavLocation + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	if (!CanAttack(Attacker, Target, WeaponRange, Center)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackStandingSpace), false, &Attacker);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() - 2.f);
	return !Attacker.GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, Shape, Params);
}

bool AttackPositionRules::IsPathClearOfTarget(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
	const TArray<FVector>& PathPoints)
{
	// NavMesh does not include these characters as obstacles. Account for both bodies explicitly.
	const float Clearance = Attacker.GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Target.GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.f;
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
