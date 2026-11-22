#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WeaponTrail.generated.h"

class UMaterialInterface;

/** Samples the equipped mesh during the authored swing; does not cause damage. */
UCLASS(meta=(DisplayName="Weapon Trail"))
class BOLDERSDEMO_API UAnimNotifyState_WeaponTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Blade endpoints, in the equipped static mesh's local space (cm).
	UPROPERTY(EditAnywhere, Category="Trail")
	FVector BladeRoot = FVector(0, 0, 34);

	UPROPERTY(EditAnywhere, Category="Trail")
	FVector BladeTip = FVector(0, 0, 139);

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin="0.03", ClampMax="0.35"))
	float FadeTime = .11f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin="0", ClampMax="20"))
	float TipWidth = 8.f;

	UPROPERTY(EditAnywhere, Category="Trail")
	FLinearColor Tint = FLinearColor(1.f, .65f, .29f);

	UPROPERTY(EditAnywhere, Category="Trail")
	TObjectPtr<UMaterialInterface> TrailMaterial;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("Weapon Trail"); }
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
