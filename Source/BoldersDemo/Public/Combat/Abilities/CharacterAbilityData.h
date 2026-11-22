#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterAbilityData.generated.h"

class UBasicAttackAbility;

// Authoring data only: runtime ability specs and handles belong to the ASC.
UCLASS(BlueprintType)
class BOLDERSDEMO_API UCharacterAbilityData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TSubclassOf<UBasicAttackAbility> BasicAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities", meta=(ClampMin="1", UIMin="1"))
	int32 BasicAttackLevel = 1;
};
