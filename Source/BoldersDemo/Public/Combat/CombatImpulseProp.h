#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatImpulseProp.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class BOLDERSDEMO_API ACombatImpulseProp : public AActor
{
	GENERATED_BODY()

public:
	ACombatImpulseProp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Physics") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;
};
