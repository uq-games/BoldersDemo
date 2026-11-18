#pragma once

#include "Combat/AttackLoadout.h"

class ABaseCharacter;
class AActor;
class UPrimitiveComponent;

struct FElementalHitBatch
{
	TSet<TWeakObjectPtr<ABaseCharacter>> DirectTargets;
	TSet<TWeakObjectPtr<ABaseCharacter>> ConductedTargets;
	TSet<TWeakObjectPtr<ABaseCharacter>> PushedTargets;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> PushedBodies;
};

namespace AttackResolution
{
	bool ApplyHit(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module, float Damage,
		FElementalHitBatch* Batch = nullptr, const AActor* Ignore = nullptr);
	void ApplyPressure(ABaseCharacter& Source, const FVector& Center, const FVector& FallbackDirection,
		float Radius, float Strength, FElementalHitBatch& Batch, const AActor* Ignore = nullptr);
	bool HasClearPath(const ABaseCharacter& Source, const ABaseCharacter& Target, const FVector& Origin, const AActor* Ignore = nullptr);
	TArray<ABaseCharacter*> FindTargets(ABaseCharacter& Source, const FVector& Origin, float Radius);
	FVector GroundPoint(const ABaseCharacter& Character);
	void ReleaseMelee(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module);
	void WarnSkyStrike(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module);
}
