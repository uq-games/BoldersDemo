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
	if (!PC || MustSpectate(PC)) return;
	AssignStartingSlot(*PC);
	if (IsValid(PC->GetAssignedCharacter())) return;
	if (SelectionCatalog) return; // Keep the camera pawn, wait for this player's confirmed choice.
	SpawnAssignedCharacter(PC,nullptr);
}

void ABoldersGameMode::AssignStartingSlot(ABoldersPlayerController& PC)
{
	if (PC.GetPlayerSlot() != 0) return;

	// A remote connection must leave the host's slot free on a listen server.
	bool bSlotTaken[2] = {GetNetMode() == NM_ListenServer && !PC.GetLocalPlayer(), false};
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const auto* Other = Cast<ABoldersPlayerController>(It->Get());
		if (!IsValid(Other)) continue;
		const int32 Index = Other->GetPlayerSlot() - 1;
		if (Index >= 0 && Index < UE_ARRAY_COUNT(bSlotTaken)) bSlotTaken[Index] = true;
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(bSlotTaken); ++Index)
	{
		if (!bSlotTaken[Index])
		{
			PC.SetPlayerSlot(Index + 1);
			return;
		}
	}
}

bool ABoldersGameMode::ChooseStartingClass(ABoldersPlayerController* PC, FName ClassId)
{
	if (!HasAuthority() || !IsValid(PC) || PC->GetWorld()!=GetWorld() || PC->GetPlayerSlot() == 0
		|| PC->GetAssignedCharacter() || !SelectionCatalog) return false;
	for (const UClassSelectionData* Definition : SelectionCatalog->Classes)
		if (Definition && Definition->ClassId == ClassId && Definition->CharacterClass)
			return SpawnAssignedCharacter(PC,Definition->CharacterClass);
	return false;
}

bool ABoldersGameMode::SpawnAssignedCharacter(ABoldersPlayerController* PC, TSubclassOf<ABaseCharacter> RequestedClass)
{
	const int32 SlotIndex = PC->GetPlayerSlot() - 1;
	if (SlotIndex < 0) return false;

	TArray<ATargetPoint*> Starts;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("WarriorSpawn"))) Starts.Add(*It);
	Starts.Sort([](const ATargetPoint& A, const ATargetPoint& B) { return A.GetName() < B.GetName(); });
	if (!Starts.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("WarriorSpawn: No target point for %dP."), PC->GetPlayerSlot());
		return false;
	}

	TSubclassOf<ABaseCharacter> CharacterClass = RequestedClass;
	if (!CharacterClass)
	{
		CharacterClass = StartingCharacterClasses.IsValidIndex(SlotIndex) && StartingCharacterClasses[SlotIndex]
			? StartingCharacterClasses[SlotIndex].Get() : WarriorClass.Get();
	}
	if (!CharacterClass || CharacterClass->HasAnyClassFlags(CLASS_Abstract)) return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	auto* Character = GetWorld()->SpawnActor<ABaseCharacter>(CharacterClass, Starts[SlotIndex]->GetActorTransform(), Params);
	if (!Character) return false;
	Character->PlayerSlot = PC->GetPlayerSlot();
	PC->AssignCharacter(Character);
	UE_LOG(LogTemp, Log, TEXT("WarriorSpawn: %dP (%s) assigned %s"),
		PC->GetPlayerSlot(), *PC->GetName(), *Character->GetName());
	return true;
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
		PC->SetPlayerSlot(0);
	}
	Super::Logout(Exiting);
}
