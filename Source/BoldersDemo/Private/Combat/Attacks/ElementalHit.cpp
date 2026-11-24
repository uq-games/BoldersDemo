#include "Combat/Attacks/AttackResolution.h"

#include "AbilitySystemComponent.h"
#include "Characters/BaseCharacter.h"

namespace
{
	EElementalAura AuraFor(EDamageElement Element)
	{
		switch (Element)
		{
		case EDamageElement::Fire: return EElementalAura::Heat;
		case EDamageElement::Ice: return EElementalAura::Chill;
		case EDamageElement::Water: return EElementalAura::Wet;
		case EDamageElement::Lightning: return EElementalAura::Charge;
		default: return EElementalAura::None;
		}
	}

	struct FReactionRule
	{
		EElementalAura First;
		EElementalAura Second;
		EElementalReaction Reaction;
		float Multiplier;
	};

	const FReactionRule* FindReaction(EElementalAura Existing, EElementalAura Incoming)
	{
		using A = EElementalAura;
		using R = EElementalReaction;
		static constexpr FReactionRule Rules[] = {
			{A::Frozen, A::None, R::Shatter, 1.35f},
			{A::Frozen, A::Heat, R::Melt, 1.4f},
			{A::Frozen, A::Charge, R::Fracture, 1.2f},
			{A::Heat, A::Wet, R::Steam, 1.25f},
			{A::Heat, A::Chill, R::Melt, 1.4f},
			{A::Wet, A::Chill, R::Freeze, 1.f},
			{A::Wet, A::Charge, R::Conduct, 1.2f},
			{A::Heat, A::Charge, R::Overload, 1.25f},
			{A::Chill, A::Charge, R::Fracture, 1.2f},
		};
		if (Existing == A::None) return nullptr;
		for (const auto& Rule : Rules)
			if ((Rule.First == Existing && Rule.Second == Incoming) || (Rule.Second == Existing && Rule.First == Incoming))
				return &Rule;
		return nullptr;
	}

	void Conduct(ABaseCharacter& Source, ABaseCharacter& Primer, float BaseDamage, FElementalHitBatch& Batch, const AActor* Ignore)
	{
		const FVector Origin = Primer.GetActorLocation();
		constexpr float ChainRadius = 280.f;
		auto Nearby = AttackResolution::FindTargets(Source, Origin, ChainRadius);
		Nearby.Sort([&Origin](const ABaseCharacter& A, const ABaseCharacter& B)
		{
			return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation());
		});
		int32 Arcs = 0;
		for (ABaseCharacter* Other : Nearby)
		{
			if (Other == &Primer || Batch.DirectTargets.Contains(Other) || Batch.ConductedTargets.Contains(Other)
				|| Other->GetElementalAura() != EElementalAura::Wet
				|| FVector::DistSquared(Origin, Other->GetActorLocation()) > FMath::Square(ChainRadius)
				|| !AttackResolution::HasClearPath(Source, *Other, Origin, Ignore)) continue;
			if (!Other->ReceiveElementDamage(&Source, BaseDamage * .42f, EDamageElement::Lightning)) continue;
			Other->ClearElementalAura(EElementalAura::Wet);
			Batch.ConductedTargets.Add(Other);
			Source.MulticastElementalReaction(EElementalReaction::Conduct, Other->GetActorLocation(), Origin);
			if (++Arcs == 2) break;
		}
	}
}

bool AttackResolution::ApplyHit(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module, float Damage,
	FElementalHitBatch* Batch, const AActor* Ignore, TOptional<EDamageElement> ElementOverride)
{
	const EDamageElement Element = ElementOverride.Get(Module.Element);
	const EElementalAura Existing = Target.GetElementalAura();
	const EElementalAura Incoming = AuraFor(Element);
	const auto* Rule = FindReaction(Existing, Incoming);
	if (!Target.ReceiveElementDamage(&Source, Damage * (Rule ? Rule->Multiplier : 1.f), Element)) return false;
	if (!Rule)
	{
		if (Module.Pattern == EAttackPattern::Freeze || (Existing == EElementalAura::Frozen && Incoming == EElementalAura::Chill))
		{
			if (Target.IsAlive())
			{
				Target.ApplyFreezeFrom(&Source);
				Source.MulticastElementalReaction(EElementalReaction::Freeze, Target.GetActorLocation(), Target.GetActorLocation());
			}
		}
		else if (Incoming != EElementalAura::None && Existing != EElementalAura::Frozen)
		{
			Target.ApplyElementalAura(&Source, Incoming);
		}
		return true;
	}

	Target.ClearElementalAura(Existing);
	const FVector Position = Target.GetActorLocation();
	FElementalHitBatch LocalBatch;
	FElementalHitBatch& HitBatch = Batch ? *Batch : LocalBatch;
	if (Rule->Reaction == EElementalReaction::Freeze) Target.ApplyFreezeFrom(&Source);
	if (Rule->Reaction == EElementalReaction::Overload)
		ApplyPressure(Source, Position, (Position - Source.GetActorLocation()).GetSafeNormal2D(), 190.f, 540.f, HitBatch, Ignore);
	if (Rule->Reaction == EElementalReaction::Conduct) Conduct(Source, Target, Damage, HitBatch, Ignore);
	Source.MulticastElementalReaction(Rule->Reaction, Position, Position);
	UE_LOG(LogTemp, Log, TEXT("ElementalReaction: %s on %s"), *UEnum::GetValueAsString(Rule->Reaction), *Target.GetName());
	return true;
}

EElementalReaction AttackResolution::ApplyEnvironmentHit(AActor& Source, UAbilitySystemComponent& SourceASC,
	ABaseCharacter& Target, EDamageElement Element, float Damage)
{
	if (!Source.HasAuthority() || !Target.HasAuthority() || !Target.IsAlive()
		|| Target.GetFaction() == ECharacterFaction::Neutral || SourceASC.GetAvatarActor() != &Source)
		return EElementalReaction::None;

	const EElementalAura Existing = Target.GetElementalAura();
	const EElementalAura Incoming = AuraFor(Element);
	const auto* Rule = FindReaction(Existing, Incoming);
	if (Damage > 0.f && !Target.ReceiveEnvironmentDamage(Source, SourceASC,
		Damage * (Rule ? Rule->Multiplier : 1.f), Element)) return EElementalReaction::None;
	if (!Rule)
	{
		if (Incoming != EElementalAura::None && Existing != EElementalAura::Frozen)
			Target.ApplyEnvironmentAura(Source, SourceASC, Incoming);
		return EElementalReaction::None;
	}
	Target.ClearElementalAura(Existing);
	if (Rule->Reaction == EElementalReaction::Freeze)
		Target.ApplyEnvironmentAura(Source, SourceASC, EElementalAura::Frozen);
	UE_LOG(LogTemp, Log, TEXT("EnvironmentReaction: %s on %s"), *UEnum::GetValueAsString(Rule->Reaction), *Target.GetName());
	return Rule->Reaction;
}
