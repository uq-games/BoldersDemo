#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/Positioning/AttackPositionRules.h"
#include "Characters/Roles/WarriorCharacter.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Engine/World.h"
#include "Game/VolcanoEncounter.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVolcanoAttackPathTest, "BoldersDemo.Combat.VolcanoAttackPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVolcanoAttackPathTest::RunTest(const FString& Parameters)
{
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Editor test world"), World)) return false;
	auto* Encounter = World->SpawnActor<AVolcanoEncounter>();
	auto* Vent = World->SpawnActor<AVolcanoHazard>();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Hero = World->SpawnActor<AWarriorCharacter>(FVector(-450, 0, 92), FRotator::ZeroRotator, Spawn);
	if (!Encounter || !Vent || !Hero) return false;
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	ASC->AddAttributeSetSubobject(CastChecked<UBoldersAttributeSet>(Hero->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
	ASC->InitAbilityActorInfo(Hero, Hero);
	ASC->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), 100.f);
	if (!TestTrue(TEXT("A real hit primes the vent"), Vent->ReceiveAttack(Hero, EDamageElement::Physical))) return false;

	const FVector Start(-450, 0, 0);
	const FVector End(450, 0, 0);
	const FVector Inside(0, 0, 0);
	const AttackPositionRules::FApproachWarnings ApproachWarnings(*Hero);
	TestTrue(TEXT("Both approach endpoints are safe"), ApproachWarnings.IsPointSafe(Start) && ApproachWarnings.IsPointSafe(End));
	TestFalse(TEXT("The shortcut crosses the vent warning"), ApproachWarnings.IsPathSafe({Start, End}));
	TestTrue(TEXT("The path around the warning is safe"), ApproachWarnings.IsPathSafe(
		{Start, FVector(-450, 340, 0), FVector(450, 340, 0), End}));
	TestTrue(TEXT("A route can leave an area already occupied"), ApproachWarnings.IsPathSafe({Inside, End}));
	TestFalse(TEXT("The same route cannot re-enter after clearing the warning"),
		ApproachWarnings.IsPathSafe({Inside, Start, End}));
	TestTrue(TEXT("Water quenches the vent"), Vent->ReceiveAttack(Hero, EDamageElement::Water));
	TestTrue(TEXT("The path opens when the threat is gone"), AttackPositionRules::FApproachWarnings(*Hero).IsPathSafe({Start, End}));
	AttackResolution::TriggerHazardsInArea(*Hero, FVector(120, 0, 300), 100.f, EDamageElement::Fire);
	TestEqual(TEXT("An attack on a different elevation leaves the vent alone"), Vent->GetHazardState(), EVolcanoHazardState::Cooling);
	AttackResolution::TriggerHazardsInArea(*Hero, FVector(120, 0, 35), 100.f, EDamageElement::Fire, Vent);
	TestEqual(TEXT("An already processed direct hit is excluded"), Vent->GetHazardState(), EVolcanoHazardState::Cooling);
	AttackResolution::TriggerHazardsInArea(*Hero, FVector(120, 0, 35), 100.f, EDamageElement::Fire);
	TestEqual(TEXT("A nearby fire impact primes an exposed vent"), Vent->GetHazardState(), EVolcanoHazardState::Warning);
	Vent->DeactivateForEncounterEnd();
	TestFalse(TEXT("The encounter result clears the danger field"), Vent->ThreatensGround(Inside));
	TestFalse(TEXT("The stopped vent cannot be rearmed"), Vent->ReceiveAttack(Hero, EDamageElement::Fire));

	Vent->Kind = EVolcanoHazardKind::Puddle;
	TestTrue(TEXT("Splash reaches the long edge of the cooling trough"), Vent->IntersectsAreaImpact(FVector(195, 0, 20), 20.f));
	TestFalse(TEXT("The trough does not extend to the old circular width"), Vent->IntersectsAreaImpact(FVector(0, 155, 20), 20.f));
	return true;
}

#endif
