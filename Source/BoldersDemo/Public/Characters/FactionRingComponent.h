#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "FactionRingComponent.generated.h"

class UMaterialInstanceDynamic;

// Presentation only: owns no faction/selection gameplay state and never replicates.
UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class BOLDERSDEMO_API UFactionRingComponent : public UDecalComponent
{
	GENERATED_BODY()
public:
	UFactionRingComponent();
	void SetSelected(bool bSelected);
	UFUNCTION(BlueprintPure, Category="Faction Ring")
	bool IsSelected() const { return bLocallySelected; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring")
	FLinearColor PlayerColor = FLinearColor(0.02f, 0.3f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring")
	FLinearColor EnemyColor = FLinearColor(1.f, 0.025f, 0.01f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring", meta=(ClampMin="0"))
	float NormalBrightness = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring", meta=(ClampMin="0"))
	float SelectedBrightness = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring", meta=(ClampMin="1", Units="cm"))
	float Radius = 55.f;
	// Shallow volume limits spill onto nearby walls and lower floors.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring|Ground", meta=(ClampMin="1", Units="cm"))
	float ProjectionDepth = 24.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring|Ground", meta=(ClampMin="0", Units="cm"))
	float TraceAboveFeet = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring|Ground", meta=(ClampMin="1", Units="cm"))
	float TraceBelowFeet = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring|Ground", meta=(ClampMin="0", Units="cm"))
	float SurfaceOffset = 1.f;
	// Only stationary characters on fixed ground use this interval; moving supports never do.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Faction Ring|Ground", meta=(ClampMin="0", ClampMax="1", Units="s"))
	float StationaryTraceInterval = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class FFactionRingTest;
	void UpdateRing(float DeltaTime = 0.f);
	void UpdateAppearance();
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterialInstance;
	bool bLocallySelected = false;
	FLinearColor AppliedColor = FLinearColor::Transparent;
	float AppliedBrightness = -1.f;
	TWeakObjectPtr<UPrimitiveComponent> CachedGround;
	FTransform CachedGroundTransform;
	FVector LastTracedFeet = FVector::ZeroVector;
	float TimeSinceGroundTrace = 0.f;
#if WITH_DEV_AUTOMATION_TESTS
	uint32 GroundTraceCount = 0;
#endif
};
