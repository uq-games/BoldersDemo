#include "Characters/BoldersCombatMovementComponent.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"

FVector UBoldersCombatMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	const auto* Character = Cast<ABaseCharacter>(GetCharacterOwner());
	if (Character && Character->HasDirectCombatInput() && Character->Combat
		&& Character->Combat->GetOrderPhase() == EAttackOrderPhase::Windup)
		return FVector::ZeroVector;

	return Super::ConstrainInputAcceleration(InputAcceleration);
}
