#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BoldersGameMode.generated.h"

class AWarriorCharacter;
class ABaseCharacter;
class UClassSelectionCatalog;
class ABoldersPlayerController;
UCLASS()
class BOLDERSDEMO_API ABoldersGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	bool ChooseStartingClass(ABoldersPlayerController* PC, FName ClassId);
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
protected:
	UPROPERTY(EditDefaultsOnly, Category="Characters") TObjectPtr<UClassSelectionCatalog> SelectionCatalog;
	// Server-authored starting classes, indexed by the existing spawn/player slot.
	// Empty entries use WarriorClass, preserving existing maps.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Characters")
	TArray<TSubclassOf<ABaseCharacter>> StartingCharacterClasses;
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TSubclassOf<AWarriorCharacter> WarriorClass;
private:
	bool SpawnAssignedCharacter(ABoldersPlayerController* PC, TSubclassOf<ABaseCharacter> RequestedClass);
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> OccupiedStarts;
};
