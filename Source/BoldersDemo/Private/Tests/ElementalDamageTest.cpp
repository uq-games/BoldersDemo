#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Characters/EnemyCharacter.h"
#include "Characters/WarriorCharacter.h"
#include "Combat/BoldersAttributeSet.h"
#include "Combat/BoldersDamageEffect.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElementalDamageTest, "BoldersDemo.Combat.ElementalDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FElementalDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Warrior = World->SpawnActor<AWarriorCharacter>(Spawn);
	auto* Enemy = World->SpawnActor<AEnemyCharacter>(Spawn);
	if (!Warrior || !Enemy) return false;
	auto* SourceASC = Warrior->GetAbilitySystemComponent();
	auto* TargetASC = Enemy->GetAbilitySystemComponent();
	// A newly created editor test map never performs the gameplay-world component initialization.
	SourceASC->AddAttributeSetSubobject(CastChecked<UBoldersAttributeSet>(Warrior->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
	TargetASC->AddAttributeSetSubobject(CastChecked<UBoldersAttributeSet>(Enemy->GetDefaultSubobjectByName(TEXT("AttributeSet"))));
	SourceASC->InitAbilityActorInfo(Warrior, Warrior);
	TargetASC->InitAbilityActorInfo(Enemy, Enemy);

	const FGameplayAttribute Attributes[] = {
		UBoldersAttributeSet::GetPhysicalResistanceAttribute(),
		UBoldersAttributeSet::GetFireResistanceAttribute(),
		UBoldersAttributeSet::GetIceResistanceAttribute(),
		UBoldersAttributeSet::GetWaterResistanceAttribute(),
		UBoldersAttributeSet::GetLightningResistanceAttribute()
	};
	const EDamageElement Elements[] = {
		EDamageElement::Physical, EDamageElement::Fire, EDamageElement::Ice, EDamageElement::Water, EDamageElement::Lightning
	};
	const auto Health = UBoldersAttributeSet::GetHealthAttribute();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Attributes); ++Index)
	{
		TargetASC->SetNumericAttributeBase(Health, 100.f);
		TargetASC->SetNumericAttributeBase(Attributes[Index], .4f);
		TestTrue(TEXT("Hostile damage applied"), Enemy->ReceiveElementDamage(Warrior, 50.f, Elements[Index]));
		TestEqual(GetDamageElementTag(Elements[Index]).ToString(), Enemy->GetHealth(), 70.f);
		const int32 Different = (Index + 1) % UE_ARRAY_COUNT(Attributes);
		TestTrue(TEXT("Different element applied"), Enemy->ReceiveElementDamage(Warrior, 20.f, Elements[Different]));
		TestEqual(TEXT("Other element ignores current resistance"), Enemy->GetHealth(), 50.f);
		TargetASC->SetNumericAttributeBase(Attributes[Index], 0.f);
	}

	TargetASC->SetNumericAttributeBase(Health, 100.f);
	TargetASC->SetNumericAttributeBase(Attributes[0], -.4f);
	TestTrue(TEXT("Legacy physical hit applied"), Enemy->ReceiveDamageFrom(Warrior, 25.f));
	TestEqual(TEXT("Physical vulnerability also applies to legacy attacks"), Enemy->GetHealth(), 65.f);
	TestFalse(TEXT("Friendly damage rejected"), Warrior->ReceiveElementDamage(Warrior, 20.f, EDamageElement::Fire));

	Enemy->ApplyFreezeFrom(Warrior);
	TestTrue(TEXT("GameplayEffect grants Frozen"), Enemy->IsFrozen());
	TestFalse(TEXT("Frozen unit cannot start a move"), Enemy->RequestMoveToLocation(FVector(200, 0, 0)));
	Enemy->ApplyFreezeFrom(Warrior);
	FGameplayTagContainer FrozenTag(TAG_State_Frozen);
	const auto FrozenEffects = TargetASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FrozenTag));
	TestEqual(TEXT("Freeze refreshes the existing effect"), FrozenEffects.Num(), 1);
	for (const auto Handle : FrozenEffects) TargetASC->RemoveActiveGameplayEffect(Handle);
	TestFalse(TEXT("Removing freeze releases its tag"), Enemy->IsFrozen());

	Warrior->Destroy();
	Enemy->Destroy();
	return true;
}

#endif
