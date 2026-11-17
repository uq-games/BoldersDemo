#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BoldersAttributeSet.generated.h"

#define BOLDERS_ATTRIBUTE_ACCESSORS(Class, Property) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(Class, Property) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Property) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Property) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Property)

UCLASS()
class BOLDERSDEMO_API UBoldersAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	UBoldersAttributeSet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes")
	FGameplayAttributeData Health;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, Health)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes")
	FGameplayAttributeData MaxHealth;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, MaxHealth)
protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
