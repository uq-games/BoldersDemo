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
	// Fractional resistance. Negative values increase damage of the corresponding element.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PhysicalResistance, Category="Attributes|Resistance")
	FGameplayAttributeData PhysicalResistance;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, PhysicalResistance)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FireResistance, Category="Attributes|Resistance")
	FGameplayAttributeData FireResistance;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, FireResistance)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IceResistance, Category="Attributes|Resistance")
	FGameplayAttributeData IceResistance;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, IceResistance)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_WaterResistance, Category="Attributes|Resistance")
	FGameplayAttributeData WaterResistance;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, WaterResistance)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LightningResistance, Category="Attributes|Resistance")
	FGameplayAttributeData LightningResistance;
	BOLDERS_ATTRIBUTE_ACCESSORS(UBoldersAttributeSet, LightningResistance)
	static void ClampResistance(const FGameplayAttribute& Attribute, float& Value);
protected:
	UFUNCTION() void OnRep_PhysicalResistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FireResistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_IceResistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_WaterResistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LightningResistance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
