#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Characters/Presentation/FactionRingComponent.h"
#include "Characters/Roles/WarriorCharacter.h"
#include "Characters/Roles/EnemyCharacter.h"
#include "Player/BoldersPlayerController.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactionRingTest, "BoldersDemo.Presentation.FactionRing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFactionRingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Hero = World->SpawnActor<AWarriorCharacter>(Spawn);
	auto* Enemy = World->SpawnActor<AEnemyCharacter>(Spawn);
	auto* Neutral = World->SpawnActor<ABaseCharacter>(Spawn);
	auto* PC = World->SpawnActor<ABoldersPlayerController>(Spawn);
	auto* Floor = World->SpawnActor<AStaticMeshActor>(Spawn);
	auto* Material = LoadObject<UMaterialInterface>(nullptr, CombatGuideStyle::FootprintMaterialPath);
	if (!TestNotNull(TEXT("Configured decal material exists"), Material)) return false;
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetActorScale3D(FVector(10.f, 10.f, 0.2f));
	Floor->SetActorLocation(FVector(0, 0, -10));
	Hero->SetActorLocation(FVector(0, 0, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Enemy->SetActorLocation(FVector(200, 0, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Hero->FactionRing->SetDecalMaterial(Material);
	Enemy->FactionRing->SetDecalMaterial(Material);
	Neutral->FactionRing->SetDecalMaterial(Material);
	Hero->FactionRing->UpdateRing(); Enemy->FactionRing->UpdateRing(); Neutral->FactionRing->UpdateRing();
	TestTrue(TEXT("Flat ground ring visible"), Hero->FactionRing->IsVisible());
	TestTrue(TEXT("Ring sits at ground, not capsule center"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().Z, 1., 0.1));
	TestFalse(TEXT("Neutral ring hidden"), Neutral->FactionRing->IsVisible());
	if (!TestNotNull(TEXT("Hero owns MID"), Hero->FactionRing->RingMaterialInstance.Get())) return false;
	TestTrue(TEXT("Separate MIDs per character"), Hero->FactionRing->RingMaterialInstance != Enemy->FactionRing->RingMaterialInstance);
	TestEqual(TEXT("Player blue"), Hero->FactionRing->RingMaterialInstance->K2_GetVectorParameterValue(TEXT("RingColor")), Hero->FactionRing->PlayerColor);
	TestEqual(TEXT("Enemy red"), Enemy->FactionRing->RingMaterialInstance->K2_GetVectorParameterValue(TEXT("RingColor")), Enemy->FactionRing->EnemyColor);
	TestEqual(TEXT("Enemy uses broken-edge mask"), Enemy->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("MarkStyle")), 2.f);
	Hero->PlayerSlot = 1;
	Hero->FactionRing->UpdateRing();
	TestEqual(TEXT("Host uses continuous edge"), Hero->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("MarkStyle")), 0.f);
	Hero->PlayerSlot = 2;
	Hero->FactionRing->UpdateRing();
	TestEqual(TEXT("Guest uses paired gaps"), Hero->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("MarkStyle")), 1.f);
	TestEqual(TEXT("Guest is amber"), Hero->FactionRing->RingMaterialInstance->K2_GetVectorParameterValue(TEXT("RingColor")),
		FLinearColor(1.f, .76f, .23f));
	Hero->PlayerSlot = 0;
	// Count actual queries, rather than just testing a scheduling predicate.
	Floor->SetMobility(EComponentMobility::Static);
	Hero->FactionRing->UpdateRing(0.2f);
	uint32 Before = Hero->FactionRing->GroundTraceCount;
	for (int32 Frame = 0; Frame < 60; ++Frame) Hero->FactionRing->UpdateRing(1.f / 60.f);
	const uint32 IdleQueries = Hero->FactionRing->GroundTraceCount - Before;
	TestTrue(TEXT("Static idle uses about five queries per second"), IdleQueries >= 4 && IdleQueries <= 6);
	Before = Hero->FactionRing->GroundTraceCount;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Hero->AddActorWorldOffset(FVector(0.1, 0, 0));
		Hero->FactionRing->UpdateRing(1.f / 60.f);
	}
	TestEqual(TEXT("Moving keeps one query per frame"), Hero->FactionRing->GroundTraceCount - Before, 60u);
	AddInfo(FString::Printf(TEXT("Ground queries at simulated 60 FPS: idle=%u/60, moving=60/60"), IdleQueries));
	// A newly encountered step must update immediately, before the idle interval expires.
	auto* Step = World->SpawnActor<AStaticMeshActor>(Spawn);
	Step->GetStaticMeshComponent()->SetStaticMesh(Floor->GetStaticMeshComponent()->GetStaticMesh());
	Step->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Step->SetActorScale3D(FVector(1, 1, 0.4));
	Step->SetActorLocation(FVector(250, 0, 20));
	Hero->SetActorLocation(FVector(250, 0, 40 + Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Hero->FactionRing->UpdateRing(1.f / 60.f);
	TestTrue(TEXT("Step up updates immediately"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().Z, 41., 0.1));
	Hero->SetActorLocation(FVector(0, 0, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Hero->FactionRing->UpdateRing(1.f / 60.f);
	TestTrue(TEXT("Step down updates immediately"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().Z, 1., 0.1));
	Step->Destroy();
	// Movable platforms stay at full rate even with no actor velocity or displacement.
	Floor->SetMobility(EComponentMobility::Movable);
	Before = Hero->FactionRing->GroundTraceCount;
	for (int32 Frame = 0; Frame < 60; ++Frame) Hero->FactionRing->UpdateRing(1.f / 60.f);
	TestEqual(TEXT("Idle on movable support is never throttled"), Hero->FactionRing->GroundTraceCount - Before, 60u);
	Floor->SetActorLocation(FVector(0, 0, -5));
	Hero->FactionRing->UpdateRing(1.f / 60.f);
	TestTrue(TEXT("Platform movement updates without waiting for actor motion"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().Z, 6., 0.1));
	Floor->SetActorRotation(FRotator(10, 0, 0));
	Hero->FactionRing->UpdateRing(1.f / 60.f);
	TestTrue(TEXT("Platform rotation updates immediately"), FVector::DotProduct(-Hero->FactionRing->GetForwardVector(), Floor->GetActorUpVector()) > 0.999);
	Floor->SetActorRotation(FRotator::ZeroRotator);
	Floor->SetActorLocation(FVector(0, 0, -10));
	PC->InitInputSystem(); PC->SetAsLocalPlayerController(); PC->AssignCharacter(Hero);
	PC->UpdateSelectedCharacter(Hero); PC->UpdateSelectedEnemy(Enemy);
	TestEqual(TEXT("Hero selection brightens material"), Hero->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("RingBrightness")), Hero->FactionRing->SelectedBrightness);
	TestEqual(TEXT("Enemy selection brightens material"), Enemy->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("RingBrightness")), Enemy->FactionRing->SelectedBrightness);
	PC->ClearSelection();
	TestEqual(TEXT("Cancel restores normal brightness"), Hero->FactionRing->RingMaterialInstance->K2_GetScalarParameterValue(TEXT("RingBrightness")), Hero->FactionRing->NormalBrightness);
	TestFalse(TEXT("Cancel clears enemy highlight"), Enemy->FactionRing->IsSelected());
	Hero->SetActorLocation(FVector(100, 0, 180));
	Floor->SetActorLocation(FVector(0, 0, 80));
	Hero->FactionRing->UpdateRing();
	TestTrue(TEXT("Follows character horizontally"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().X, 100., 0.1));
	TestTrue(TEXT("Tracks elevated floor"), FMath::IsNearlyEqual(Hero->FactionRing->GetComponentLocation().Z, 91., 0.1));
	Floor->SetActorRotation(FRotator(20, 0, 0));
	Hero->FactionRing->UpdateRing();
	TestTrue(TEXT("Slope remains visible"), Hero->FactionRing->IsVisible());
	TestTrue(TEXT("Projection follows slope normal"), FVector::DotProduct(-Hero->FactionRing->GetForwardVector(), Floor->GetActorUpVector()) > 0.999);
	Hero->SetActorLocation(FVector(2000, 0, 180)); Hero->FactionRing->UpdateRing();
	TestFalse(TEXT("No ground hides ring"), Hero->FactionRing->IsVisible());
	Hero->SetActorLocation(FVector(100, 0, 180)); Hero->FactionRing->UpdateRing();
	TestTrue(TEXT("Ground return restores ring"), Hero->FactionRing->IsVisible());
	PC->UpdateSelectedCharacter(Hero);
	CastChecked<UBoldersAttributeSet>(Hero->GetDefaultSubobjectByName(TEXT("AttributeSet")))->InitHealth(0.f);
	PC->PlayerTick(0.f); Hero->FactionRing->UpdateRing();
	TestFalse(TEXT("Death clears selection"), Hero->FactionRing->IsSelected());
	TestFalse(TEXT("Death hides decal"), Hero->FactionRing->IsVisible());
	PC->Destroy(); Hero->Destroy(); Enemy->Destroy(); Neutral->Destroy(); Floor->Destroy();
	return true;
}
#endif
