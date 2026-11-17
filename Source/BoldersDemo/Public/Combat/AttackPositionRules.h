#pragma once

#include "CoreMinimal.h"

class ABaseCharacter;

// Shared geometry rules. No target state, navigation requests or animation ownership.
namespace AttackPositionRules
{
	float GetReach(const ABaseCharacter& Attacker, const ABaseCharacter& Target, float WeaponRange);
	bool CanAttack(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
		float WeaponRange, const FVector& ActorCenter);
	bool CanStandAndAttack(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
		float WeaponRange, const FVector& NavLocation);
	bool IsPathClearOfTarget(const ABaseCharacter& Attacker, const ABaseCharacter& Target,
		const TArray<FVector>& PathPoints);
}
