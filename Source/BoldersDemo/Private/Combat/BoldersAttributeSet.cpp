#include "Combat/BoldersAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UBoldersAttributeSet::UBoldersAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
}

void UBoldersAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, PhysicalResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, FireResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, IceResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, WaterResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBoldersAttributeSet, LightningResistance, COND_None, REPNOTIFY_Always);
}

void UBoldersAttributeSet::ClampResistance(const FGameplayAttribute& Attribute, float& Value)
{
	if (Attribute == GetPhysicalResistanceAttribute()
		|| Attribute == GetFireResistanceAttribute()
		|| Attribute == GetIceResistanceAttribute()
		|| Attribute == GetWaterResistanceAttribute()
		|| Attribute == GetLightningResistanceAttribute())
		Value = FMath::IsFinite(Value) ? FMath::Clamp(Value, -.75f, .9f) : 0.f;
}

void UBoldersAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampResistance(Attribute, NewValue);
	if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	if (Attribute == GetMaxHealthAttribute()) NewValue = FMath::Max(NewValue, 1.f);
}

void UBoldersAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampResistance(Attribute, NewValue);
	if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	if (Attribute == GetMaxHealthAttribute()) NewValue = FMath::Max(NewValue, 1.f);
}

void UBoldersAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetHealthAttribute() || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
}

void UBoldersAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, Health, OldValue);
}
void UBoldersAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, MaxHealth, OldValue);
}

void UBoldersAttributeSet::OnRep_PhysicalResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, PhysicalResistance, OldValue);
}

void UBoldersAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, FireResistance, OldValue);
}

void UBoldersAttributeSet::OnRep_IceResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, IceResistance, OldValue);
}

void UBoldersAttributeSet::OnRep_WaterResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, WaterResistance, OldValue);
}

void UBoldersAttributeSet::OnRep_LightningResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBoldersAttributeSet, LightningResistance, OldValue);
}
