#include "Game/BoldersGameMode.h"
#include "AIController.h"
#include "Characters/WarriorCharacter.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "Player/BoldersPlayerController.h"

void ABoldersGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	auto* PC = Cast<ABoldersPlayerController>(NewPlayer);
	if (!PC || IsValid(PC->GetAssignedCharacter())) return;
	if (!WarriorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("WarriorSpawn: WarriorClass is not configured."));
		return;
	}
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
		auto* Warrior = GetWorld()->SpawnActor<AWarriorCharacter>(WarriorClass, Start->GetActorTransform(), Params);
		if (!Warrior) continue;
		Warrior->PlayerSlot = Starts.IndexOfByKey(Start) + 1;
		PC->AssignCharacter(Warrior);
		OccupiedStarts.Add(PC, Start);
		UE_LOG(LogTemp, Log, TEXT("WarriorSpawn: %s assigned %s"), *PC->GetName(), *Warrior->GetName());
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("WarriorSpawn: No available WarriorSpawn target point for %s."), *PC->GetName());
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
