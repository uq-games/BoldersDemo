#include "UI/Combat/BoldersHUD.h"

#include "AI/Positioning/AttackPositionRules.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Roles/ArcherCharacter.h"
#include "Characters/Roles/MageCharacter.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Equipment/Weapon.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/BoldersPlayerController.h"

void ABoldersHUD::UpdateAttackGuide(const ABaseCharacter* Character, const ABoldersPlayerController* PC, bool bVisible)
{
	const FAttackModule* Module = Character ? Character->Combat->GetActiveModule() : nullptr;
	const AWeapon* Weapon = Character ? Character->Combat->GetWeapon() : nullptr;
	if (!bVisible || !PC || !PC->IsAttackRangeVisible() || !IsValid(Character) || !Character->IsAlive()
		|| Character->IsHidden() || (!Module && !Weapon) || !AttackGuideMaterial)
	{
		if (AttackGuide) AttackGuide->SetVisibility(false);
		if (ImpactGuide) ImpactGuide->SetVisibility(false);
		return;
	}

	const auto MakeGuide = [this](FName Name, int32 SortOrder)
	{
		auto* Decal = NewObject<UDecalComponent>(this, Name);
		Decal->SetDecalMaterial(AttackGuideMaterial);
		Decal->SetAbsolute(true, true, true);
		Decal->SetSortOrder(SortOrder);
		Decal->FadeScreenSize = 0.f;
		Decal->SetVisibility(false);
		Decal->RegisterComponent();
		return Decal;
	};
	if (!AttackGuide)
	{
		// HUD actors start hidden, including their world-space components.
		SetActorHiddenInGame(false);
		AttackGuide = MakeGuide(TEXT("AttackReach"), CombatGuideStyle::ReachSort);
		AttackGuideMID = AttackGuide->CreateDynamicMaterialInstance();
	}

	const float Range = Module ? Module->Range : Weapon->AttackRange;
	const FName ModuleId = Module ? Module->Id : NAME_None;
	const float Reach = AttackPositionRules::GetSurfaceReach(*Character, Range);
	const FVector Feet = Character->GetActorLocation()
		- FVector::UpVector * Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bReady = !Character->IsFrozen() && Character->Combat->GetCooldownRemaining(ModuleId) <= 0.f;
	FLinearColor Tint(.31f, .80f, .98f);
	if (Character->IsA<AArcherCharacter>()) Tint = FLinearColor(.35f, .96f, .72f);
	else if (Character->IsA<AMageCharacter>()) Tint = FLinearColor(.59f, .68f, 1.f);
	if (!bReady) Tint = FMath::Lerp(Tint, FLinearColor(.55f, .60f, .66f), .66f);
	const bool bMelee = Range < 250.f;
	CombatGuideStyle::PlaceGroundDecal(*AttackGuide, Feet, FVector::ForwardVector, Reach, Reach, bMelee ? 64.f : 123.f);
	if (AttackGuideMID)
	{
		AttackGuideMID->SetVectorParameterValue(TEXT("RingColor"), Tint);
		AttackGuideMID->SetScalarParameterValue(TEXT("RingBrightness"), bReady ? .83f : .36f);
		AttackGuideMID->SetScalarParameterValue(TEXT("MarkStyle"), bMelee ? 0.f : 1.f);
	}
	AttackGuide->SetVisibility(true);

	const AActor* Target = PC->GetCursorTarget();
	const bool bShowImpact = bReady && Module && Module->Pattern == EAttackPattern::SkyStrike && IsValid(Target)
		&& Character->Combat->GetOrderPhase() != EAttackOrderPhase::Windup
		&& AttackPositionRules::CanAttack(*Character, *Target, Range, Character->GetActorLocation());
	if (!bShowImpact)
	{
		if (ImpactGuide) ImpactGuide->SetVisibility(false);
		return;
	}

	if (!ImpactGuide)
	{
		ImpactGuide = MakeGuide(TEXT("AttackImpactPreview"), CombatGuideStyle::ReachSort);
		ImpactGuideMID = ImpactGuide->CreateDynamicMaterialInstance();
	}
	const auto* TargetCharacter = Cast<ABaseCharacter>(Target);
	const FVector Ground = TargetCharacter ? AttackResolution::GroundPoint(*TargetCharacter) : Target->GetActorLocation();
	CombatGuideStyle::PlaceGroundDecal(*ImpactGuide, Ground, FVector::ForwardVector, Module->Radius, Module->Radius, 85.f);
	if (ImpactGuideMID)
	{
		ImpactGuideMID->SetScalarParameterValue(TEXT("MarkStyle"), 2.f);
		ImpactGuideMID->SetVectorParameterValue(TEXT("RingColor"), FMath::Lerp(Tint, CombatGuideStyle::ElementColor(Module->Element), .28f));
	}
	ImpactGuide->SetVisibility(true);
}

