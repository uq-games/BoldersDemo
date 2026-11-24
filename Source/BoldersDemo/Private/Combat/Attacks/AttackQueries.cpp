#include "Combat/Attacks/AttackResolution.h"

#include "Characters/BaseCharacter.h"
#include "Combat/Environment/CombatCollision.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using BoldersCollision::SolidObjects;

void AttackResolution::TriggerHazardsInArea(ABaseCharacter& Source, const FVector& Center, float Radius,
	EDamageElement Element, const AActor* AlreadyHit)
{
	if (!Source.HasAuthority() || !Source.IsAlive() || Center.ContainsNaN() || Radius <= 0.f) return;
	UWorld* World = Source.GetWorld();
	FCollisionQueryParams Sight(SCENE_QUERY_STAT(HazardAreaExposure), false, &Source);
	const FCollisionObjectQueryParams Cover(ECC_WorldStatic);
	for (TActorIterator<AVolcanoHazard> It(World); It; ++It)
	{
		AVolcanoHazard* Hazard = *It;
		if (Hazard == AlreadyHit || !Hazard->CanReceiveAttack() || !Hazard->IntersectsAreaImpact(Center, Radius)) continue;
		const FVector Contact = Hazard->GetActorLocation() + FVector(0, 0, 38);
		if (World->LineTraceTestByObjectType(Center, Contact, Cover, Sight)) continue;
		if (Hazard->ReceiveAttack(&Source, Element))
			UE_LOG(LogTemp, Log, TEXT("Foundry impact: %s affected %s"), *Source.GetName(), *Hazard->GetName());
	}
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
	const FVector Start = Feet + FVector(0, 0, 12);
	const FVector End = Feet - FVector(0, 0, 70);
	if (Character.GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (!Cast<AVolcanoHazard>(Hit.GetActor())) return Hit.ImpactPoint + FVector(0, 0, 3);
		if (Character.GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			return Hit.ImpactPoint + FVector(0, 0, 3);
	}
	return Feet + FVector(0, 0, 3);
}
