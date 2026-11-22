#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarriorBladeTrail.generated.h"

class AWeapon;
class UAnimNotifyState_WeaponTrail;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

// One transient, local world-space ribbon. It outlives the notify only long enough to fade.
UCLASS(NotBlueprintable, Transient)
class AWarriorBladeTrail : public AActor
{
	GENERATED_BODY()

public:
	AWarriorBladeTrail();
	void Initialize(AWeapon& Weapon, USkeletalMeshComponent& Pose, const UAnimNotifyState_WeaponTrail& Style);
	void Stop(const UAnimNotifyState_WeaponTrail& Style);
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FSample
	{
		FVector Root;
		FVector Tip;
		double Time;
	};

	void Sample(double Now);
	void Rebuild(double Now);

	UPROPERTY(VisibleAnywhere, Category="Trail")
	TObjectPtr<UProceduralMeshComponent> Ribbon;

	TWeakObjectPtr<UStaticMeshComponent> Blade;
	TWeakObjectPtr<UStaticMesh> OriginalMesh;
	TWeakObjectPtr<const UAnimNotifyState_WeaponTrail> Notify;
	FVector BladeRoot;
	FVector BladeTip;
	float FadeTime = .11f;
	float TipWidth = 8.f;
	bool bSampling = true;
	TArray<FSample> Samples;
	FTransform LastPose;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
};
