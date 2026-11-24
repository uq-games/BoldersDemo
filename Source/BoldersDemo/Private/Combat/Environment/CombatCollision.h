#pragma once

#include "Engine/EngineTypes.h"

namespace BoldersCollision
{
	// CombatInteractable in DefaultEngine.ini. Use for targeting and projectile contact.
	inline constexpr ECollisionChannel Interactable = ECC_GameTraceChannel1;

	inline const FCollisionObjectQueryParams& SolidObjects()
	{
		static const FCollisionObjectQueryParams Objects(
			ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic) | ECC_TO_BITFIELD(ECC_PhysicsBody));
		return Objects;
	}
}
