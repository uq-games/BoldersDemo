#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterPreview.generated.h"

class UClassSelectionData;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

// Pure local presentation actor: no CharacterMovement, AIController, ASC or combat weapon actors.
UCLASS()
class BOLDERSDEMO_API ACharacterPreview : public AActor
{
	GENERATED_BODY()
public:
	ACharacterPreview();
	virtual void Tick(float DeltaSeconds) override;
	void Configure(UClassSelectionData* Data);
	void SetDisplayState(int32 State) { DisplayState = FMath::Clamp(State, 0, 2); }
	UFUNCTION(BlueprintPure) int32 GetDisplayState() const { return DisplayState; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> CharacterMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Pedestal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Weapon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Offhand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> UpperString;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LowerString;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UClassSelectionData> Definition;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	int32 DisplayState = 0;
};
