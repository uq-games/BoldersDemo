#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ElementalReactionTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EElementalAura : uint8
{
	None, Heat, Chill, Wet, Charge, Frozen
};

UENUM(BlueprintType)
enum class EElementalReaction : uint8
{
	None, Steam, Melt, Freeze, Conduct, Overload, Fracture, Shatter
};

USTRUCT(BlueprintType)
struct FReactionAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction") EElementalReaction Reaction = EElementalReaction::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction") TObjectPtr<UMaterialInterface> Burst;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction") TObjectPtr<UMaterialInterface> Ring;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction") TObjectPtr<UMaterialInterface> Beam;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction") TObjectPtr<UNiagaraSystem> Particles;
};

UCLASS(BlueprintType)
class BOLDERSDEMO_API UElementalReactionAppearance : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reactions")
	TArray<FReactionAppearance> Reactions;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reactions") TObjectPtr<UStaticMesh> CrystalMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reactions") TObjectPtr<UMaterialInterface> CrystalMaterial;
};
