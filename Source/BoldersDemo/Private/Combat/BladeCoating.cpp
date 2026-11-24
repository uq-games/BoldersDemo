#include "Combat/CharacterCombatComponent.h"

#include "Characters/Roles/WarriorCharacter.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Combat/Equipment/Weapon.h"
#include "Combat/Presentation/AttackPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	constexpr float CoatingDuration = 9.f;
	constexpr float DipCooldown = 11.f;

	bool IsSword(const FAttackModule* Module)
	{
		return Module && Module->Element == EDamageElement::Physical
			&& (Module->Pattern == EAttackPattern::SingleMelee || Module->Pattern == EAttackPattern::Cleave);
	}
}

const AVolcanoHazard* UCharacterCombatComponent::FindBladeDipSource(EDamageElement& Element, FVector& Contact) const
{
	Element = EDamageElement::Physical;
	if (!Character || !Character->IsA<AWarriorCharacter>() || !Character->IsAlive() || !IsSword(GetActiveModule())) return nullptr;

	const FVector Feet = Character->GetActorLocation()
		- FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const AVolcanoHazard* BestSource = nullptr;
	double ClosestSquared = FMath::Square(105.f + Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(BladeDipSight), false, Character);
	const FCollisionObjectQueryParams Cover(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic)
		| ECC_TO_BITFIELD(ECC_PhysicsBody));

	for (TActorIterator<AVolcanoHazard> It(GetWorld()); It; ++It)
	{
		FVector Candidate;
		const EDamageElement Sample = It->GetBladeDipContact(Feet, Candidate);
		if (Sample == EDamageElement::Physical || FMath::Abs(Feet.Z - Candidate.Z) > 72.f) continue;
		const double Distance = FVector::DistSquared(Feet, Candidate);
		if (Distance > ClosestSquared || GetWorld()->LineTraceTestByObjectType(
			Feet + FVector(0, 0, 50), Candidate + FVector(0, 0, 14), Cover, Query)) continue;

		BestSource = *It;
		ClosestSquared = Distance;
		Contact = Candidate;
		Element = Sample;
	}
	return BestSource;
}

bool UCharacterCombatComponent::TryDipBlade()
{
	if (!Character || !Character->HasAuthority() || !IsValid(Weapon) || Character->IsFrozen()
		|| GetBladeDipCooldown() > 0.f || GetOrderPhase() == EAttackOrderPhase::Windup) return false;
	EDamageElement Element;
	FVector Contact;
	const AVolcanoHazard* Basin = FindBladeDipSource(Element, Contact);
	if (!Basin) return false;

	const float Now = GetCombatTime();
	CancelAttack();
	BladeCoating = {Element, Now + CoatingDuration, Now + DipCooldown};
	GetWorld()->GetTimerManager().SetTimer(BladeCoatingTimer, this, &ThisClass::ClearBladeCoating, CoatingDuration, false);
	OnRep_BladeCoating();
	MulticastBladeCoatingEffect(Element, Contact, true);
	Character->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("BladeDip: %s took %s from %s"), *Character->GetName(), *UEnum::GetValueAsString(Element), *Basin->GetName());
	return true;
}

float UCharacterCombatComponent::GetBladeCoatingRemaining() const
{
	return FMath::Max(0.f, BladeCoating.ExpiresAt - GetCombatTime());
}

EDamageElement UCharacterCombatComponent::GetBladeCoating() const
{
	return GetBladeCoatingRemaining() > 0.f ? BladeCoating.Element : EDamageElement::Physical;
}

EDamageElement UCharacterCombatComponent::GetEffectiveAttackElement(const FAttackModule* Module) const
{
	if (!Module) return EDamageElement::Physical;
	return IsSword(Module) ? GetBladeCoating() : Module->Element;
}

float UCharacterCombatComponent::GetBladeDipCooldown() const
{
	return FMath::Max(0.f, BladeCoating.DipReadyAt - GetCombatTime());
}

void UCharacterCombatComponent::ClearBladeCoating()
{
	if (!Character || !Character->HasAuthority() || BladeCoating.Element == EDamageElement::Physical) return;
	GetWorld()->GetTimerManager().ClearTimer(BladeCoatingTimer);
	BladeCoating.Element = EDamageElement::Physical;
	BladeCoating.ExpiresAt = 0.f;
	OnRep_BladeCoating();
	Character->ForceNetUpdate();
}

void UCharacterCombatComponent::OnRep_BladeCoating()
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(Weapon)) return;
	const FAttackModule* Module = GetActiveModule();
	const EDamageElement Display = IsSword(Module) ? BladeCoating.Element : EDamageElement::Physical;
	Weapon->SetBladeCoating(Display, Module);
}

void UCharacterCombatComponent::SpendBladeCoating(EDamageElement Element, const FVector& Contact)
{
	if (!Character || !Character->HasAuthority() || GetBladeCoating() != Element || Element == EDamageElement::Physical) return;
	MulticastBladeCoatingEffect(Element, Contact, false);
	ClearBladeCoating();
}

void UCharacterCombatComponent::MulticastBladeCoatingEffect_Implementation(EDamageElement Element, FVector_NetQuantize Contact, bool bDip)
{
	if (!Character || GetNetMode() == NM_DedicatedServer) return;
	const FVector Blade = bDip && IsValid(Weapon) ? Weapon->Mesh->Bounds.Origin : FVector(Contact);
	AttackPresentation::PlayBladeCoating(*Character, Element, Contact, Blade);
}
