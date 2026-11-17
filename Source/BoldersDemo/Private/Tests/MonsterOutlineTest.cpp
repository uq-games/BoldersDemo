#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Player/BoldersPlayerController.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/WarriorCharacter.h"
#include "Combat/BoldersAttributeSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterOutlineStateTest, "BoldersDemo.Presentation.MonsterOutline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonsterOutlineStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* PC = World->SpawnActor<ABoldersPlayerController>(Spawn);
	auto* Hero = World->SpawnActor<AWarriorCharacter>(Spawn);
	auto* First = World->SpawnActor<AEnemyCharacter>(Spawn);
	auto* Second = World->SpawnActor<AEnemyCharacter>(Spawn);
	if (!PC || !Hero || !First || !Second) return false;
	// Spawned editor-world controllers do not receive the game login initialization.
	PC->InitInputSystem();
	PC->SetAsLocalPlayerController();
	PC->AssignCharacter(Hero);
	TestTrue(TEXT("Controller is local"), PC->IsLocalController());
	TestFalse(TEXT("Friendly character cannot be selected as enemy"), PC->CanSelectEnemy(Hero));
	TestTrue(TEXT("Living monster is selectable without selecting hero first"), PC->CanSelectEnemy(First));
	PC->UpdateHoveredCharacter(First);
	TestTrue(TEXT("Hover enables custom depth"), First->GetMesh()->bRenderCustomDepth);
	TestEqual(TEXT("Hover is yellow"), First->GetMesh()->CustomDepthStencilValue, 200);
	PC->UpdateSelectedEnemy(First);
	TestEqual(TEXT("Selection overrides hover with red"), First->GetMesh()->CustomDepthStencilValue, 201);
	PC->UpdateHoveredCharacter(nullptr);
	TestTrue(TEXT("Selection persists after mouse leaves"), First->GetMesh()->bRenderCustomDepth);
	PC->UpdateHoveredCharacter(Second);
	TestEqual(TEXT("Other hovered monster remains yellow"), Second->GetMesh()->CustomDepthStencilValue, 200);
	PC->UpdateSelectedEnemy(Second);
	TestFalse(TEXT("Switch clears previous selection"), First->GetMesh()->bRenderCustomDepth);
	TestEqual(TEXT("New selection is red"), Second->GetMesh()->CustomDepthStencilValue, 201);
	PC->UpdateSelectedEnemy(nullptr);
	TestEqual(TEXT("Deselected but hovered monster returns to yellow"), Second->GetMesh()->CustomDepthStencilValue, 200);
	PC->UpdateSelectedEnemy(Second);
	auto* Attributes = CastChecked<UBoldersAttributeSet>(Second->GetDefaultSubobjectByName(TEXT("AttributeSet")));
	Attributes->InitHealth(0.f);
	PC->PlayerTick(0.f);
	TestFalse(TEXT("Dead selection is cleared"), PC->SelectedEnemy.IsValid());
	TestFalse(TEXT("Dead monster loses outline"), Second->GetMesh()->bRenderCustomDepth);
	PC->UpdateHoveredCharacter(First);
	PC->UpdateSelectedEnemy(First);
	PC->AssignCharacter(nullptr);
	TestFalse(TEXT("Reassignment clears selection"), PC->SelectedEnemy.IsValid());
	TestFalse(TEXT("Reassignment clears outline"), First->GetMesh()->bRenderCustomDepth);
	PC->AssignCharacter(Hero);
	PC->UpdateSelectedEnemy(First);
	First->Destroy();
	PC->PlayerTick(0.f);
	TestFalse(TEXT("Destroyed target is safe"), PC->SelectedEnemy.IsValid());
	PC->Destroy(); Hero->Destroy(); Second->Destroy();
	return true;
}
#endif
