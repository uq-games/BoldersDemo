#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "BoldersDamageEffect.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_HealthDelta);

UCLASS()
class BOLDERSDEMO_API UBoldersDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBoldersDamageEffect();
};
