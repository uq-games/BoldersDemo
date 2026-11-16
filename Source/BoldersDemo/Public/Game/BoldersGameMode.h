#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BoldersGameMode.generated.h"

class AWarriorCharacter;
UCLASS()
class BOLDERSDEMO_API ABoldersGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
protected:
	UPROPERTY(EditDefaultsOnly, Category="Characters")
	TSubclassOf<AWarriorCharacter> WarriorClass;
private:
	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AActor>> OccupiedStarts;
};
