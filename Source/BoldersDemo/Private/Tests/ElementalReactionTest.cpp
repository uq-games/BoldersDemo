#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "../Combat/AttackResolution.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/WarriorCharacter.h"
#include "Combat/BoldersAttributeSet.h"
#include "Combat/CombatImpulseProp.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElementalReactionTest, "BoldersDemo.Combat.ElementalReactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FElementalReactionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Source = World->SpawnActor<AWarriorCharacter>(FVector(-250, 0, 75), FRotator::ZeroRotator, Spawn);
	auto* Victim = World->SpawnActor<AEnemyCharacter>(FVector(0, 0, 75), FRotator::ZeroRotator, Spawn);
	if (!Source || !Victim) return false;
	for (ABaseCharacter* Character : TArray<ABaseCharacter*>{Source, Victim})
	{
		auto* ASC = Character->GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(CastChecked<UBoldersAttributeSet>(Character->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
		ASC->InitAbilityActorInfo(Character, Character);
	}

	using E = EDamageElement;
	using A = EElementalAura;
	const struct FCombination { E First; E Second; float Damage; A Result; } Combinations[] = {
		{E::Water, E::Fire, 12.5f, A::None},
		{E::Fire, E::Ice, 14.f, A::None},
		{E::Water, E::Ice, 10.f, A::Frozen},
		{E::Water, E::Lightning, 12.f, A::None},
		{E::Fire, E::Lightning, 12.5f, A::None},
		{E::Ice, E::Lightning, 12.f, A::None}
	};
	const auto Health = UBoldersAttributeSet::GetHealthAttribute();
	FAttackModule Attack;
	for (const auto& Entry : Combinations)
	{
		for (const bool bReverse : {false, true})
		{
			Victim->ClearElementalAura(Victim->GetElementalAura());
			Victim->GetAbilitySystemComponent()->SetNumericAttributeBase(Health, 100.f);
			Attack.Element = bReverse ? Entry.Second : Entry.First;
			TestTrue(TEXT("Primer hits"), AttackResolution::ApplyHit(*Source, *Victim, Attack, 10.f));
			TestEqual(TEXT("Unprimed spell deals base damage"), Victim->GetHealth(), 90.f);
			TestTrue(TEXT("Primer applies aura"), Victim->GetElementalAura() != A::None);
			Attack.Element = bReverse ? Entry.First : Entry.Second;
			TestTrue(TEXT("Reaction hit succeeds"), AttackResolution::ApplyHit(*Source, *Victim, Attack, 10.f));
			TestEqual(TEXT("Reaction damage"), Victim->GetHealth(), 90.f - Entry.Damage);
			TestEqual(TEXT("Reaction consumes or freezes"), Victim->GetElementalAura(), Entry.Result);
		}
	}
	Victim->GetAbilitySystemComponent()->SetNumericAttributeBase(Health, 100.f);
	Victim->ApplyFreezeFrom(Source);
	Attack.Element = E::Physical;
	TestTrue(TEXT("Physical attack shatters frozen target"), AttackResolution::ApplyHit(*Source, *Victim, Attack, 20.f));
	TestEqual(TEXT("Shatter deals 135 percent physical damage"), Victim->GetHealth(), 73.f);
	TestFalse(TEXT("Frozen tag is cleared immediately"), Victim->IsFrozen());

	auto* OpenProp = World->SpawnActor<ACombatImpulseProp>(FVector(65, 100, 75), FRotator::ZeroRotator, Spawn);
	auto* CoveredProp = World->SpawnActor<ACombatImpulseProp>(FVector(-110, -100, 75), FRotator::ZeroRotator, Spawn);
	auto* Wall = World->SpawnActor<AStaticMeshActor>(FVector(5, -72, 75), FRotator::ZeroRotator, Spawn);
	if (OpenProp && CoveredProp && Wall)
	{
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		FElementalHitBatch Batch;
		AttackResolution::ApplyPressure(*Source, FVector(30, 0, 75), FVector::ForwardVector, 250.f, 900.f, Batch);
		TestTrue(TEXT("Exposed physics body receives pressure"), Batch.PushedBodies.Contains(OpenProp->Body.Get()));
		TestFalse(TEXT("Wall shields physics body"), Batch.PushedBodies.Contains(CoveredProp->Body.Get()));
		TestTrue(TEXT("Enemy receives displacement pressure"), Batch.PushedTargets.Contains(Victim));
		TestFalse(TEXT("Caster never knocks itself back"), Batch.PushedTargets.Contains(Source));
	}
	else AddError(TEXT("Could not spawn the pressure fixtures"));
	return true;
}

#endif
