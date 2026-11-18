#include "AttackResolution.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

	const FCollisionObjectQueryParams& SolidObjects()
	{
		static const FCollisionObjectQueryParams Objects(
			ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic) | ECC_TO_BITFIELD(ECC_PhysicsBody));
		return Objects;
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
	FElementalHitBatch* Batch, const AActor* Ignore)
{
	const EElementalAura Existing = Target.GetElementalAura();
	const EElementalAura Incoming = AuraFor(Module.Element);
	const auto* Rule = FindReaction(Existing, Incoming);
	if (!Target.ReceiveElementDamage(&Source, Damage * (Rule ? Rule->Multiplier : 1.f), Module.Element)) return false;
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

void AttackResolution::ApplyPressure(ABaseCharacter& Source, const FVector& Center, const FVector& FallbackDirection,
	float Radius, float Strength, FElementalHitBatch& Batch, const AActor* Ignore)
{
	if (!Source.HasAuthority() || Radius <= 0.f || Strength <= 0.f) return;
	const auto PressureAt = [Center, Radius, Strength, FallbackDirection](const FVector& Point)
	{
		const FVector Offset = Point - Center;
		const double Ratio = FMath::Clamp(Offset.Size() / Radius, 0.0, 1.0);
		FVector Direction = Offset.GetSafeNormal2D();
		if (Direction.IsNearlyZero()) Direction = FallbackDirection.GetSafeNormal2D();
		const float Speed = FMath::Min(Strength, 1200.f) * FMath::Square(1.f - Ratio);
		return (Direction + FVector(0, 0, .34f)) * Speed;
	};
	for (ABaseCharacter* Target : FindTargets(Source, Center, Radius))
	{
		const FVector Velocity = PressureAt(Target->GetActorLocation());
		if (Velocity.SizeSquared() < FMath::Square(30.f) || Batch.PushedTargets.Contains(Target)
			|| !HasClearPath(Source, *Target, Center, Ignore)) continue;
		Batch.PushedTargets.Add(Target);
		Target->ApplyCombatImpulse(Velocity);
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BlastPressure), false, &Source);
	Params.AddIgnoredActor(Ignore);
	const FCollisionObjectQueryParams PhysicsTypes(ECC_TO_BITFIELD(ECC_PhysicsBody) | ECC_TO_BITFIELD(ECC_WorldDynamic));
	Source.GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, PhysicsTypes,
		FCollisionShape::MakeSphere(Radius), Params);
	for (const auto& Result : Overlaps)
	{
		UPrimitiveComponent* Body = Result.GetComponent();
		if (!Body || !Body->IsSimulatingPhysics() || Batch.PushedBodies.Contains(Body)) continue;
		const FVector CenterOfMass = Body->GetCenterOfMass();
		const FVector Velocity = PressureAt(CenterOfMass);
		if (Velocity.SizeSquared() < FMath::Square(8.f)) continue;
		FCollisionQueryParams Sight = Params;
		Sight.AddIgnoredComponent(Body);
		// A projectile may strike this body directly. Other solid geometry still blocks the pressure.
		if (Source.GetWorld()->LineTraceTestByObjectType(Center, CenterOfMass, SolidObjects(), Sight)) continue;
		Batch.PushedBodies.Add(Body);
		const FVector Contact = CenterOfMass + FVector(0, 0, 15.f);
		Body->AddImpulseAtLocation(Velocity * FMath::Clamp(Body->GetMass(), 1.f, 180.f), Contact);
		if (AActor* Owner = Body->GetOwner()) Owner->ForceNetUpdate();
	}
}

bool AttackResolution::HasClearPath(const ABaseCharacter& Source, const ABaseCharacter& Target, const FVector& Origin, const AActor* Ignore)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ModuleSight), false, &Source);
	Params.AddIgnoredActor(&Target);
	Params.AddIgnoredActor(Ignore);
	// Characters may stand inside the same blast/wave. Solid geometry still shields them.
	return !Source.GetWorld()->LineTraceTestByObjectType(Origin, Target.GetActorLocation(), SolidObjects(), Params);
}

TArray<ABaseCharacter*> AttackResolution::FindTargets(ABaseCharacter& Source, const FVector& Origin, float Radius)
{
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ModuleTargets), false, &Source);
	Source.GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius), Params);
	TArray<ABaseCharacter*> Targets;
	for (const auto& Result : Overlaps)
	{
		auto* Candidate = Cast<ABaseCharacter>(Result.GetActor());
		if (Source.IsHostileTo(Candidate) && Candidate->IsAlive()) Targets.AddUnique(Candidate);
	}
	return Targets;
}

FVector AttackResolution::GroundPoint(const ABaseCharacter& Character)
{
	const FVector Center = Character.GetActorLocation();
	const FVector Feet = Center - FVector(0, 0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ModuleGround), false, &Character);
	FHitResult Hit;
	if (Character.GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 12), Feet - FVector(0, 0, 70),
		ECC_Visibility, Params)) return Hit.ImpactPoint + FVector(0, 0, 3);
	return Feet + FVector(0, 0, 3);
}

void AttackResolution::ReleaseMelee(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module)
{
	const FVector Origin = Source.GetActorLocation();
	const FVector Direction = (Target.GetActorLocation() - Origin).GetSafeNormal2D();
	Source.MulticastModuleVisual(Module.Id, EAttackVisualStage::Cast, Origin, Origin + Direction * Module.Range);

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

void AttackResolution::WarnSkyStrike(ABaseCharacter& Source, ABaseCharacter& Target, const FAttackModule& Module)
{
	const FVector Aim = GroundPoint(Target);
	const FName Id = Module.Id;
	Source.MulticastModuleVisual(Id, EAttackVisualStage::GroundWarning, Aim, Aim);
	auto Strike = FTimerDelegate::CreateWeakLambda(&Source, [&Source, Aim, Id]
	{
		if (!Source.IsAlive()) return;
		const FAttackModule* Attack = Source.Combat->FindModule(Id);
		if (!Attack) return;
		// Trace from above the impact site. An overhang can shield the units below.
		const FVector Sky = Aim + FVector(0, 0, 980);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SkyStrike), false, &Source);
		FHitResult Roof;
		const FCollisionObjectQueryParams WorldObjects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
		const bool bCovered = Source.GetWorld()->LineTraceSingleByObjectType(Roof, Sky, Aim, WorldObjects, Params);
		const FVector Endpoint = bCovered ? Roof.ImpactPoint + FVector(0, 0, 4) : Aim;
		Source.MulticastModuleVisual(Id, EAttackVisualStage::SkyImpact, Endpoint, Sky);
		if (bCovered) return;
		const FVector Center = Aim + FVector(0, 0, 48);
		TArray<ABaseCharacter*> Victims;
		FElementalHitBatch Batch;
		for (ABaseCharacter* Victim : FindTargets(Source, Center, Attack->Radius))
		{
			const FVector Offset = Victim->GetActorLocation() - Center;
			if (Offset.SizeSquared2D() <= FMath::Square(Attack->Radius)
				&& FMath::Abs(Offset.Z) <= 105.f && HasClearPath(Source, *Victim, Center))
			{
				Victims.Add(Victim);
				Batch.DirectTargets.Add(Victim);
			}
		}
		for (ABaseCharacter* Victim : Victims) ApplyHit(Source, *Victim, *Attack, Attack->Damage, &Batch);
	});
	FTimerHandle Handle;
	Source.GetWorldTimerManager().SetTimer(Handle, Strike, FMath::Max(.05f, Module.LaunchDelay), false);
}
