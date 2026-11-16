#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KayKitEquipmentComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Cosmetic equipment for the three project KayKit meshes. No projectile or combat authority. */
UCLASS(ClassGroup=(Characters))
class BOLDERSDEMO_API UKayKitEquipmentComponent : public UActorComponent
{

	GENERATED_BODY()

public:
	UKayKitEquipmentComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Also usable after changing a character mesh at runtime. */
	UFUNCTION(BlueprintCallable, Category="Character|Equipment")
	void RefreshEquipment();

	UFUNCTION(BlueprintCallable, Category="Character|Equipment")
	void UpdateBowString();

	UFUNCTION(BlueprintPure, Category="Character|Equipment")
	TArray<FVector> GetBowStringPoints() const;

private:
	UStaticMeshComponent* MakePart(UStaticMesh* Asset, FName Socket);
	void ClearEquipment();
	USkeletalMeshComponent* GetCharacterMesh() const;

	UPROPERTY() TObjectPtr<UStaticMesh> SwordAsset;
	UPROPERTY() TObjectPtr<UStaticMesh> ShieldAsset;
	UPROPERTY() TObjectPtr<UStaticMesh> BowAsset;
	UPROPERTY() TObjectPtr<UStaticMesh> StaffAsset;
	UPROPERTY() TObjectPtr<UStaticMesh> StringAsset;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Bow;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UpperString;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LowerString;
	TWeakObjectPtr<USkeletalMesh> EquippedMesh;
	TArray<FVector> StringPoints;
};
