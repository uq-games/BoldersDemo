#include "Combat/BoldersDamageEffect.h"
#include "Combat/BoldersAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HealthDelta, "Data.HealthDelta");

UBoldersDamageEffect::UBoldersDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UBoldersAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = TAG_Data_HealthDelta;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
