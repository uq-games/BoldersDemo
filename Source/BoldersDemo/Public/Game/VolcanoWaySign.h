#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VolcanoWaySign.generated.h"

class ABaseCharacter;
class UMaterialInterface;
class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EVolcanoWaySignKind : uint8
{
	Entry, CastingRoad, Core, Cooling
};

// Level-authored route information. Reading a sign only changes the local HUD.
UCLASS()
class BOLDERSDEMO_API AVolcanoWaySign : public AActor
{
	GENERATED_BODY()

public:
	AVolcanoWaySign();
	virtual void PostRegisterAllComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	static const AVolcanoWaySign* FindReadable(const ABaseCharacter& Reader);
	bool CanRead(const ABaseCharacter& Reader) const;
	FVector GetPlaqueLocation() const;
	FText GetTitle() const;
	TArray<FText> GetInstructions() const;

	UPROPERTY(EditInstanceOnly, Category="Way Sign")
	EVolcanoWaySignKind Kind = EVolcanoWaySignKind::Entry;

private:
	void UpdatePlaqueArt();

	UPROPERTY(VisibleAnywhere, Category="Way Sign")
	TObjectPtr<UProceduralMeshComponent> PlaqueFace;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> PlaqueMaterials;
};
