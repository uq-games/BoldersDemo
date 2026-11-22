#include "Characters/BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "AI/BoldersAIController.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Effects/BoldersDamageEffect.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const { return AbilitySystemComponent; }

float ABaseCharacter::GetHealth() const { return AttributeSet->GetHealth(); }

float ABaseCharacter::GetMaxHealth() const { return AttributeSet->GetMaxHealth(); }

bool ABaseCharacter::IsAlive() const { return GetHealth() > 0.f; }

bool ABaseCharacter::IsFrozen() const { return AbilitySystemComponent->HasMatchingGameplayTag(TAG_State_Frozen); }

EElementalAura ABaseCharacter::GetElementalAura() const
{
	const EElementalAura Auras[] = {EElementalAura::Frozen, EElementalAura::Wet, EElementalAura::Chill,
		EElementalAura::Heat, EElementalAura::Charge};
	for (const EElementalAura Aura : Auras)
		if (AbilitySystemComponent->HasMatchingGameplayTag(GetElementalAuraTag(Aura))) return Aura;
	return EElementalAura::None;
}

bool ABaseCharacter::ReceiveDamageFrom(ABaseCharacter* Source, float Damage)
{
	if (!IsValid(Source)) return false;
	const FVector ToSource = (Source->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	return ReceiveDamageAt(Source, Damage, GetActorLocation() + ToSource * GetCapsuleComponent()->GetScaledCapsuleRadius());
}

bool ABaseCharacter::ReceiveDamageAt(ABaseCharacter* Source, float Damage, const FVector& Contact)
{
	if (!ReceiveElementDamage(Source, Damage, EDamageElement::Physical)) return false;
	Source->MulticastAttackVisual(Contact, FRotator::ZeroRotator, true);
	return true;
}

bool ABaseCharacter::ReceiveElementDamage(ABaseCharacter* Source, float Damage, EDamageElement Element)
{
	if (!HasAuthority() || !IsAlive() || !IsHostileTo(Source) || !Source->IsAlive()
		|| !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	auto* SourceASC = Source->GetAbilitySystemComponent();
	auto Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);
	auto Spec = SourceASC->MakeOutgoingSpec(UBoldersDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(TAG_Data_HealthDelta, -Damage);
	const FGameplayTag ElementTag = GetDamageElementTag(Element);
	Spec.Data->AddDynamicAssetTag(ElementTag);
	const float Before = GetHealth();
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComponent);
	UE_LOG(LogTemp, Log, TEXT("CombatDamage: %s -> %s health %.1f -> %.1f (%s)"),
		*Source->GetName(), *GetName(), Before, GetHealth(), *ElementTag.ToString());
	return GetHealth() < Before;
}

void ABaseCharacter::ApplyFreezeFrom(ABaseCharacter* Source)
{
	ApplyElementalAura(Source, EElementalAura::Frozen);
}

void ABaseCharacter::ApplyElementalAura(ABaseCharacter* Source, EElementalAura Aura)
{
	if (!HasAuthority() || !IsAlive() || !IsHostileTo(Source)) return;
	const auto Effect = GetElementalAuraEffect(Aura);
	if (!Effect) return;
	const EElementalAura Previous = GetElementalAura();
	auto* SourceASC = Source->GetAbilitySystemComponent();
	const auto Spec = SourceASC->MakeOutgoingSpec(Effect, 1.f, SourceASC->MakeEffectContext());
	if (Spec.IsValid() && SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComponent).IsValid()
		&& Previous != Aura)
	{
		ClearElementalAura(Previous);
	}
}

void ABaseCharacter::ClearElementalAura(EElementalAura Aura)
{
	const FGameplayTag Tag = GetElementalAuraTag(Aura);
	if (!HasAuthority() || !Tag.IsValid()) return;
	AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FGameplayTagContainer(Tag)));
}

void ABaseCharacter::ApplyCombatImpulse(const FVector& VelocityChange)
{
	if (!HasAuthority() || !IsAlive() || VelocityChange.ContainsNaN() || VelocityChange.IsNearlyZero()) return;
	StopCombatMovement();
	GetCharacterMovement()->AddImpulse(VelocityChange, true);
	ForceNetUpdate();
}

void ABaseCharacter::HandleFrozenChanged(const FGameplayTag Tag, int32 Count)
{
	UpdateFrozenVisual(Count > 0);
	if (Count <= 0 || !HasAuthority()) return;
	StopCombatMovement();
}

void ABaseCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue > 0.f && Data.NewValue < Data.OldValue)
	{
		MulticastPlayHitReaction();
		return;
	}
	if (Data.OldValue <= 0.f || Data.NewValue > 0.f) return;
	StopCombatMovement();
	GetCharacterMovement()->DisableMovement();
}

void ABaseCharacter::StopCombatMovement()
{
	Combat->CancelAttack();
	if (auto* AI = Cast<ABoldersAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
}
