#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/Positioning/AttackPositionRules.h"
#include "Characters/Roles/EnemyCharacter.h"
#include "Characters/Roles/WarriorCharacter.h"
#include "Combat/Abilities/BasicAttackAbility.h"
#include "Combat/Attacks/AttackProjectile.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Combat/Equipment/Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
	void InitCombatAttributes(ABaseCharacter& Character)
	{
		auto* ASC = Character.GetAbilitySystemComponent();
		ASC->AddAttributeSetSubobject(CastChecked<UBoldersAttributeSet>(
			Character.GetDefaultSubobjectByName(TEXT("AttributeSet"))));
		ASC->InitAbilityActorInfo(&Character, &Character);
		ASC->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), 100.f);
	}

	FActorSpawnParameters TestSpawn()
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Spawn;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVolcanoProjectileResponseTest, "BoldersDemo.Combat.VolcanoProjectileResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVolcanoProjectileResponseTest::RunTest(const FString& Parameters)
{
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Editor test world"), World)) return false;
	const FVector Origin(-350, 0, 110);
	const FActorSpawnParameters Spawn = TestSpawn();
	auto* Source = World->SpawnActor<AWarriorCharacter>(Origin, FRotator::ZeroRotator, Spawn);
	auto* Ally = World->SpawnActor<AWarriorCharacter>(Origin + FVector(120, 0, 0), FRotator::ZeroRotator, Spawn);
	auto* Enemy = World->SpawnActor<AEnemyCharacter>(FVector(240, 0, 110), FRotator::ZeroRotator, Spawn);
	if (!Source || !Ally || !Enemy) return false;
	for (auto* Member : TArray<ABaseCharacter*>{Source, Ally, Enemy}) InitCombatAttributes(*Member);

	TestTrue(TEXT("An ally at the muzzle leaves the attack sight clear"),
		AttackPositionRules::CanAttack(*Source, *Enemy, 700.f, Origin));
	auto* Shot = World->SpawnActor<AAttackProjectile>(Origin + FVector(48, 0, 0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Projectile fixture"), Shot)) return false;
	Shot->InitializeShot(Source, 20.f, FVector::ForwardVector);
	TArray<FHitResult> Contacts;
	const FComponentQueryParams Query(SCENE_QUERY_STAT(ProjectileResponseTest), Shot);
	const bool bBlocked = World->ComponentSweepMulti(Contacts, Shot->Collision, Shot->GetActorLocation(),
		Enemy->GetActorLocation(), FQuat::Identity, Query);
	TestFalse(TEXT("The ally does not block the projectile sweep"), bBlocked);
	TestTrue(TEXT("The projectile sweep touches the ally"), Contacts.ContainsByPredicate(
		[Ally](const FHitResult& Hit) { return Hit.GetActor() == Ally; }));
	// Exercise the damage callback separately since editor-world movement suppresses overlap delegates.
	Shot->OnPawnOverlap(Shot->Collision, Ally, nullptr, 0, false, FHitResult());
	TestEqual(TEXT("Ally health is unchanged"), Ally->GetHealth(), 100.f);
	TestFalse(TEXT("The ally leaves the projectile live"), Shot->bImpacted);
	Shot->OnPawnOverlap(Shot->Collision, Enemy, nullptr, 0, false, FHitResult());
	TestTrue(TEXT("Overlapping a hostile applies damage"), Enemy->GetHealth() < 100.f);

	auto* Wall = World->SpawnActor<AStaticMeshActor>(FVector(140, 0, 105), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Wall fixture"), Wall)) return false;
	auto* WallMesh = Wall->GetStaticMeshComponent();
	WallMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
	TestFalse(TEXT("Solid cover blocks the same attack"), AttackPositionRules::CanAttack(*Source, *Enemy, 700.f, Origin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVolcanoHazardResponseTest, "BoldersDemo.Combat.VolcanoHazardResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVolcanoHazardResponseTest::RunTest(const FString& Parameters)
{
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Editor test world"), World)) return false;
	const FActorSpawnParameters Spawn = TestSpawn();
	auto* Warrior = World->SpawnActor<AWarriorCharacter>(FVector(-125, 0, 92), FRotator::ZeroRotator, Spawn);
	auto* Enemy = World->SpawnActor<AEnemyCharacter>(FVector(-55, 0, 92), FRotator::ZeroRotator, Spawn);
	auto* Pool = World->SpawnActor<AVolcanoHazard>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!Warrior || !Enemy || !Pool) return false;
	InitCombatAttributes(*Warrior);
	InitCombatAttributes(*Enemy);
	Pool->Kind = EVolcanoHazardKind::Puddle;

	// Editor test worlds do not run the gameplay component's BeginPlay.
	auto& Combat = *Warrior->Combat;
	Combat.Character = Warrior;
	Combat.Weapon = World->SpawnActor<AWeapon>(Spawn);
	Combat.AttackLoadout = NewObject<UAttackLoadout>(&Combat);
	auto& Blade = Combat.AttackLoadout->Modules.AddDefaulted_GetRef();
	Blade.Id = TEXT("TestBlade");
	Blade.Pattern = EAttackPattern::SingleMelee;
	Blade.Cooldown = 1.f;
	Blade.Damage = 10.f;
	Combat.ActiveModuleId = Blade.Id;
	Combat.ReadyTimes = {0.f};
	Combat.BasicAttackHandle = Warrior->GetAbilitySystemComponent()->GiveAbility(
		FGameplayAbilitySpec(UBasicAttackAbility::StaticClass(), 1));

	using Element = EDamageElement;
	TestTrue(TEXT("A physical swing on a cooling trough is rejected"),
		Combat.RequestAttack(Pool) == EAttackOrderResult::Ineffective);
	TestEqual(TEXT("Rejecting an incompatible target preserves its cooldown"), Combat.GetCooldownRemaining(Blade.Id), 0.f);
	TestTrue(TEXT("The warrior takes water at the trough"), Combat.TryDipBlade());
	TestTrue(TEXT("Effective sword element is water"), Combat.GetEffectiveAttackElement(&Blade) == Element::Water);
	AttackResolution::ReleaseMelee(*Warrior, *Enemy, Blade);
	TestTrue(TEXT("A successful hit consumes one coating"), Combat.GetBladeCoating() == Element::Physical);
	TestTrue(TEXT("The target received the coating element"), Enemy->GetElementalAura() == EElementalAura::Wet);

	TestTrue(TEXT("Ice freezes the trough"), Pool->ReceiveAttack(Warrior, Element::Ice));
	TestFalse(TEXT("Frozen trough rejects another ice hit"), Pool->CanReactTo(Element::Ice));
	TestFalse(TEXT("Frozen trough rejects lightning"), Pool->CanReactTo(Element::Lightning));
	TestTrue(TEXT("Fire can change the frozen trough state"), Pool->ReceiveAttack(Warrior, Element::Fire));
	TestFalse(TEXT("Steam prevents a fresh change"), Pool->CanReactTo(Element::Ice));
	return true;
}

#endif
