#include "Combat/Attacks/AttackResolution.h"

#include "Characters/BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void AttackResolution::ReleaseMelee(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module)
{
	const FVector Origin = Source.GetActorLocation();
	const FVector Direction = (Target.GetActorLocation() - Origin).GetSafeNormal2D();

	TArray<ABaseCharacter*> Victims{ &Target };
	if (Module.Pattern == EAttackPattern::Cleave)
	{
		for (ABaseCharacter* Candidate : FindTargets(Source, Origin, Module.Range + Source.GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{
			const FVector Offset = Candidate->GetActorLocation() - Origin;
			if (FVector::DotProduct(Offset.GetSafeNormal2D(), Direction) >= .47
				&& HasClearPath(Source, *Candidate, Origin)) Victims.AddUnique(Candidate);
		}
	}
	for (ABaseCharacter* Victim : Victims)
	{
		if (!ApplyHit(Source, *Victim, Module, Module.Damage)) continue;
		Victim->ApplyCombatImpulse(Direction * Module.ImpactPush + FVector(0, 0, Module.ImpactPush * .22f));
		const FVector Contact = Victim->GetActorLocation() - Direction * Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		Source.MulticastModuleVisual(Module.Id, EAttackVisualStage::Impact, Contact, Contact + Direction * 80.f);
	}
}

void AttackResolution::ReleaseSkyStrike(ABaseCharacter& Source, const FAttackModule& Module, const FVector& Aim)
{
	// Trace from above the impact site. An overhang can shield the units below.
	const FVector Sky = Aim + FVector(0, 0, 980);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkyStrike), false, &Source);
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
		const FVector Offset = Victim->GetActorLocation() - Center;
		if (Offset.SizeSquared2D() <= FMath::Square(Module.Radius)
			&& FMath::Abs(Offset.Z) <= 105.f && HasClearPath(Source, *Victim, Center))
		{
			Victims.Add(Victim);
			Batch.DirectTargets.Add(Victim);
		}
	}
	for (ABaseCharacter* Victim : Victims) ApplyHit(Source, *Victim, Module, Module.Damage, &Batch);
}
