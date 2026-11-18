#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Combat/AttackLoadout.h"
#include "NativeGameplayTags.h"
#include "BoldersDamageEffect.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_HealthDelta);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Frozen);

FGameplayTag GetDamageElementTag(EDamageElement Element);
FGameplayTag GetElementalAuraTag(EElementalAura Aura);
TSubclassOf<UGameplayEffect> GetElementalAuraEffect(EElementalAura Aura);

UCLASS()
class UBoldersDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	UBoldersDamageExecution();
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
		FGameplayEffectCustomExecutionOutput& Output) const override;
};

UCLASS()
class BOLDERSDEMO_API UBoldersDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UBoldersDamageEffect();
};

UCLASS()
class UBoldersAuraEffect : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	void ConfigureAura(FGameplayTag Tag, float Seconds);
};

UCLASS()
class UBoldersFreezeEffect : public UBoldersAuraEffect
{
	GENERATED_BODY()
public:
	UBoldersFreezeEffect();
};

UCLASS()
class UBoldersHeatEffect : public UBoldersAuraEffect
{
	GENERATED_BODY()
public:
	UBoldersHeatEffect();
};

UCLASS()
class UBoldersChillEffect : public UBoldersAuraEffect
{
	GENERATED_BODY()
public:
	UBoldersChillEffect();
};

UCLASS()
class UBoldersWetEffect : public UBoldersAuraEffect
{
	GENERATED_BODY()
public:
	UBoldersWetEffect();
};

UCLASS()
class UBoldersChargeEffect : public UBoldersAuraEffect
{
	GENERATED_BODY()
public:
	UBoldersChargeEffect();
};
