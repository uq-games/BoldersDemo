#pragma once

#include "CoreMinimal.h"

class ABaseCharacter;
class AActor;
class AVolcanoEncounter;
class AVolcanoHazard;

// Shared geometry rules. No target state, navigation requests or animation ownership.
namespace AttackPositionRules
{
	// Used within one synchronous path query or movement update.
	class FApproachWarnings
	{
	public:
		explicit FApproachWarnings(const ABaseCharacter& Attacker);
		bool IsPointSafe(const FVector& GroundPosition) const;
		bool IsPathSafe(const TArray<FVector>& PathPoints) const;

	private:
		const AVolcanoEncounter* Encounter = nullptr;
		TArray<const AVolcanoHazard*, TInlineAllocator<12>> Hazards;
	};

	bool CanTarget(const ABaseCharacter& Attacker, const AActor* Target);
	FVector GetAttackPoint(const AActor& Target);
	float GetSurfaceReach(const ABaseCharacter& Attacker, float WeaponRange);
	bool IsInRange(const ABaseCharacter& Attacker, const AActor& Target, float WeaponRange, const FVector& ActorCenter);
	float GetReach(const ABaseCharacter& Attacker, const AActor& Target, float WeaponRange);
	bool CanAttack(const ABaseCharacter& Attacker, const AActor& Target,
		float WeaponRange, const FVector& ActorCenter);
	bool CanStandAndAttack(const ABaseCharacter& Attacker, const AActor& Target,
		float WeaponRange, const FVector& NavLocation);
	bool IsPathClearOfTarget(const ABaseCharacter& Attacker, const AActor& Target,
		const TArray<FVector>& PathPoints);
}
