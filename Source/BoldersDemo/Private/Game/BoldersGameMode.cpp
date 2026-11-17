#include "Game/BoldersGameMode.h"
#include "AIController.h"
#include "Characters/WarriorCharacter.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "Player/BoldersPlayerController.h"
#include "UI/ClassSelectionData.h"

void ABoldersGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	auto* PC = Cast<ABoldersPlayerController>(NewPlayer);
	if (!PC || IsValid(PC->GetAssignedCharacter())) return;
	if (SelectionCatalog) return; // Keep the camera pawn, wait for this player's confirmed choice.
	SpawnAssignedCharacter(PC,nullptr);
}

bool ABoldersGameMode::ChooseStartingClass(ABoldersPlayerController* PC, FName ClassId)
{
	if (!HasAuthority() || !IsValid(PC) || PC->GetWorld()!=GetWorld() || PC->GetAssignedCharacter() || !SelectionCatalog) return false;
	for (const UClassSelectionData* Definition : SelectionCatalog->Classes)
		if (Definition && Definition->ClassId == ClassId && Definition->CharacterClass)
			return SpawnAssignedCharacter(PC,Definition->CharacterClass);
	return false;
}

bool ABoldersGameMode::SpawnAssignedCharacter(ABoldersPlayerController* PC, TSubclassOf<ABaseCharacter> RequestedClass)
{
	TArray<ATargetPoint*> Starts;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("WarriorSpawn"))) Starts.Add(*It);
	Starts.Sort([](const ATargetPoint& A, const ATargetPoint& B) { return A.GetName() < B.GetName(); });
	for (ATargetPoint* Start : Starts)
	{
		bool bOccupied = false;
		for (const auto& Pair : OccupiedStarts)
			if (Pair.Key.IsValid() && Pair.Value == Start) { bOccupied = true; break; }
		if (bOccupied) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		const int32 SlotIndex = Starts.IndexOfByKey(Start);
		TSubclassOf<ABaseCharacter> CharacterClass = WarriorClass.Get();
		if (StartingCharacterClasses.IsValidIndex(SlotIndex) && StartingCharacterClasses[SlotIndex])
			CharacterClass = StartingCharacterClasses[SlotIndex];
		if (RequestedClass) CharacterClass=RequestedClass;
		if (!CharacterClass || CharacterClass->HasAnyClassFlags(CLASS_Abstract)) continue;
		auto* Character = GetWorld()->SpawnActor<ABaseCharacter>(CharacterClass, Start->GetActorTransform(), Params);
		if (!Character) continue;
		Character->PlayerSlot = SlotIndex + 1;
		PC->AssignCharacter(Character);
		OccupiedStarts.Add(PC, Start);
		UE_LOG(LogTemp, Log, TEXT("WarriorSpawn: %s assigned %s"), *PC->GetName(), *Character->GetName());
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("WarriorSpawn: No available WarriorSpawn target point for %s."), *PC->GetName());
	return false;
}

void ABoldersGameMode::Logout(AController* Exiting)
{
	if (auto* PC = Cast<ABoldersPlayerController>(Exiting))
	{
		if (ABaseCharacter* Character = PC->GetAssignedCharacter(); IsValid(Character))
		{
			auto* AI = Cast<AAIController>(Character->GetController());
			if (AI) { AI->StopMovement(); AI->UnPossess(); }
			Character->Destroy();
			if (AI) AI->Destroy();
		}
		PC->AssignCharacter(nullptr);
	}
	OccupiedStarts.Remove(Exiting);
	Super::Logout(Exiting);
}
