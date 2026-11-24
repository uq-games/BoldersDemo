#include "Combat/Attacks/AttackResolution.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void AttackResolution::ReleaseMelee(ABaseCharacter& Source, AActor& Target, const FAttackModule& Module)
{
	const FVector Origin = Source.GetActorLocation();
	const FVector Direction = (Target.GetActorLocation() - Origin).GetSafeNormal2D();
	const EDamageElement Element = Source.Combat->GetEffectiveAttackElement(&Module);
	const bool bCoated = Element != Module.Element;
	TOptional<FVector> CoatedContact;

	TArray<ABaseCharacter*> Victims;
	if (auto* Character = Cast<ABaseCharacter>(&Target)) Victims.Add(Character);
	if (auto* Hazard = Cast<AVolcanoHazard>(&Target); Hazard && Hazard->ReceiveAttack(&Source, Element))
	{
		const FVector Contact = Hazard->GetAttackPoint();
		CoatedContact = Contact;
		Source.MulticastModuleVisual(Module.Id, EAttackVisualStage::Impact, Contact, Contact + Direction * 80.f);
	}
	if (Module.Pattern == EAttackPattern::Cleave)
	{
		for (ABaseCharacter* Candidate : FindTargets(Source, Origin, Module.Range + Source.GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{
			const FVector Offset = Candidate->GetActorLocation() - Origin;
			if (FVector::DotProduct(Offset.GetSafeNormal2D(), Direction) >= .47
				&& HasClearPath(Source, *Candidate, Origin, &Target)) Victims.AddUnique(Candidate);
		}
	}
	FElementalHitBatch Batch;
	for (ABaseCharacter* Victim : Victims) Batch.DirectTargets.Add(Victim);
	for (ABaseCharacter* Victim : Victims)
	{
		if (!ApplyHit(Source, *Victim, Module, Module.Damage, &Batch, nullptr, Element)) continue;
		Victim->ApplyCombatImpulse(Direction * Module.ImpactPush + FVector(0, 0, Module.ImpactPush * .22f));
		const FVector Contact = Victim->GetActorLocation() - Direction * Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (!CoatedContact.IsSet()) CoatedContact = Contact;
		Source.MulticastModuleVisual(Module.Id, EAttackVisualStage::Impact, Contact, Contact + Direction * 80.f);
	}
	if (bCoated && CoatedContact.IsSet()) Source.Combat->SpendBladeCoating(Element, CoatedContact.GetValue());
}

void AttackResolution::ReleaseSkyStrike(ABaseCharacter& Source, const FAttackModule& Module, const FVector& Aim, AVolcanoHazard* Hazard)
{
	// Trace from above the impact site. An overhang can shield the units below.
	const FVector Sky = Aim + FVector(0, 0, 980);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkyStrike), false, &Source);
	Params.AddIgnoredActor(Hazard);
	FHitResult Roof;
	const FCollisionObjectQueryParams WorldObjects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
	const bool bCovered = Source.GetWorld()->LineTraceSingleByObjectType(Roof, Sky, Aim, WorldObjects, Params);
	const FVector Endpoint = bCovered ? Roof.ImpactPoint + FVector(0, 0, 4) : Aim;
	Source.MulticastModuleVisual(Module.Id, EAttackVisualStage::SkyImpact, Endpoint, Sky);
	if (bCovered) return;
	const FVector Center = Aim + FVector(0, 0, 48);
	TArray<ABaseCharacter*> Victims;
	FElementalHitBatch Batch;
	for (ABaseCharacter* Victim : FindTargets(Source, Center, Module.Radius))
	{
		const FVector Offset = GroundPoint(*Victim) - Aim;
		if (Offset.SizeSquared2D() <= FMath::Square(Module.Radius)
			&& FMath::Abs(Offset.Z) <= 105.f && HasClearPath(Source, *Victim, Center, Hazard))
		{
			Victims.Add(Victim);
			Batch.DirectTargets.Add(Victim);
		}
	}
	for (ABaseCharacter* Victim : Victims) ApplyHit(Source, *Victim, Module, Module.Damage, &Batch);
	if (IsValid(Hazard)) Hazard->ReceiveAttack(&Source, Module.Element);
	TriggerHazardsInArea(Source, Center, Module.Radius, Module.Element, Hazard);
}
