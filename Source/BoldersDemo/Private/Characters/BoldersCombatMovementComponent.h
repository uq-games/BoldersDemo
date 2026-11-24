#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "BoldersCombatMovementComponent.generated.h"

UCLASS()
class UBoldersCombatMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
};
