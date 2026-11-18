#include "Combat/BoldersDamageEffect.h"
#include "Combat/BoldersAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HealthDelta, "Data.HealthDelta");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Frozen, "State.Combat.Frozen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aura_Heat, "State.Combat.Aura.Heat");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aura_Chill, "State.Combat.Aura.Chill");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aura_Wet, "State.Combat.Aura.Wet");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aura_Charge, "State.Combat.Aura.Charge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Damage_Physical, "Damage.Element.Physical");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Damage_Fire, "Damage.Element.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Damage_Ice, "Damage.Element.Ice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Damage_Water, "Damage.Element.Water");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Damage_Lightning, "Damage.Element.Lightning");

namespace
{
	struct FElementCapture
	{
		EDamageElement Element;
		FGameplayTag Tag;
		FGameplayEffectAttributeCaptureDefinition Resistance;
	};

	const TArray<FElementCapture>& GetElements()
	{
		static const TArray<FElementCapture> Elements = {
			{EDamageElement::Physical, TAG_Damage_Physical, {UBoldersAttributeSet::GetPhysicalResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false}},
			{EDamageElement::Fire, TAG_Damage_Fire, {UBoldersAttributeSet::GetFireResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false}},
			{EDamageElement::Ice, TAG_Damage_Ice, {UBoldersAttributeSet::GetIceResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false}},
			{EDamageElement::Water, TAG_Damage_Water, {UBoldersAttributeSet::GetWaterResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false}},
			{EDamageElement::Lightning, TAG_Damage_Lightning, {UBoldersAttributeSet::GetLightningResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false}},
		};
		return Elements;
	}
}

FGameplayTag GetDamageElementTag(EDamageElement Element)
{
	for (const auto& Entry : GetElements())
		if (Entry.Element == Element) return Entry.Tag;
	return TAG_Damage_Physical;
}

UBoldersDamageExecution::UBoldersDamageExecution()
{
	// The execution CDO can be created during startup, before native tags are finalized.
	RelevantAttributesToCapture.Emplace(UBoldersAttributeSet::GetPhysicalResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RelevantAttributesToCapture.Emplace(UBoldersAttributeSet::GetFireResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RelevantAttributesToCapture.Emplace(UBoldersAttributeSet::GetIceResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RelevantAttributesToCapture.Emplace(UBoldersAttributeSet::GetWaterResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RelevantAttributesToCapture.Emplace(UBoldersAttributeSet::GetLightningResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
}

void UBoldersDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
	FGameplayEffectCustomExecutionOutput& Output) const
{
	const FGameplayEffectSpec& Spec = Params.GetOwningSpec();
	const float Delta = Spec.GetSetByCallerMagnitude(TAG_Data_HealthDelta, false, 0.f);
	if (!FMath::IsFinite(Delta) || Delta >= 0.f) return;
	const auto& Elements = GetElements();
	const auto* Element = Elements.FindByPredicate([&Spec](const FElementCapture& Entry)
	{
		return Spec.GetDynamicAssetTags().HasTagExact(Entry.Tag);
	});
	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Resistance = 0.f;
	Params.AttemptCalculateCapturedAttributeMagnitude(Element ? Element->Resistance : Elements[0].Resistance, Evaluation, Resistance);
	UBoldersAttributeSet::ClampResistance(Element ? Element->Resistance.AttributeToCapture : Elements[0].Resistance.AttributeToCapture, Resistance);
	Output.AddOutputModifier(FGameplayModifierEvaluatedData(UBoldersAttributeSet::GetHealthAttribute(),
		EGameplayModOp::Additive, Delta * (1.f - Resistance)));
}

UBoldersDamageEffect::UBoldersDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Executions.AddDefaulted_GetRef().CalculationClass = UBoldersDamageExecution::StaticClass();
}

FGameplayTag GetElementalAuraTag(EElementalAura Aura)
{
	switch (Aura)
	{
	case EElementalAura::Heat: return TAG_Aura_Heat;
	case EElementalAura::Chill: return TAG_Aura_Chill;
	case EElementalAura::Wet: return TAG_Aura_Wet;
	case EElementalAura::Charge: return TAG_Aura_Charge;
	case EElementalAura::Frozen: return TAG_State_Frozen;
	default: return FGameplayTag();
	}
}

TSubclassOf<UGameplayEffect> GetElementalAuraEffect(EElementalAura Aura)
{
	switch (Aura)
	{
	case EElementalAura::Heat: return UBoldersHeatEffect::StaticClass();
	case EElementalAura::Chill: return UBoldersChillEffect::StaticClass();
	case EElementalAura::Wet: return UBoldersWetEffect::StaticClass();
	case EElementalAura::Charge: return UBoldersChargeEffect::StaticClass();
	case EElementalAura::Frozen: return UBoldersFreezeEffect::StaticClass();
	default: return nullptr;
	}
}

void UBoldersAuraEffect::ConfigureAura(FGameplayTag Tag, float Seconds)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(Seconds);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	FInheritedTagContainer Granted;
	Granted.AddTag(Tag);
	auto* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("AuraTargetTags"));
	GEComponents.Add(Tags);
	Tags->SetAndApplyTargetTagChanges(Granted);
}

UBoldersFreezeEffect::UBoldersFreezeEffect() { ConfigureAura(TAG_State_Frozen, 1.25f); }
UBoldersHeatEffect::UBoldersHeatEffect() { ConfigureAura(TAG_Aura_Heat, 5.f); }
UBoldersChillEffect::UBoldersChillEffect() { ConfigureAura(TAG_Aura_Chill, 5.f); }
UBoldersWetEffect::UBoldersWetEffect() { ConfigureAura(TAG_Aura_Wet, 5.f); }
UBoldersChargeEffect::UBoldersChargeEffect() { ConfigureAura(TAG_Aura_Charge, 5.f); }
