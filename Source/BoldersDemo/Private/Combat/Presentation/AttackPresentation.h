#pragma once

#include "Combat/Data/AttackLoadout.h"

class AAttackProjectile;
class ABaseCharacter;

namespace AttackPresentation
{
	void Play(ABaseCharacter& Source, const FAttackModule& Module, EAttackVisualStage Stage,
		const FVector& Location, const FVector& End);
	void ConfigureProjectile(AAttackProjectile& Projectile, const FAttackModule& Module);
	AActor* PlayWindup(ABaseCharacter& Source, const FAttackModule& Module, float Duration, const FVector& Aim);
	void PlayReaction(ABaseCharacter& Source, const UElementalReactionAppearance& Appearance,
		EElementalReaction Reaction, const FVector& Location, const FVector& End);
}
