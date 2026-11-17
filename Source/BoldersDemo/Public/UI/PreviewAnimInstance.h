#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PreviewAnimInstance.generated.h"

UCLASS()
class BOLDERSDEMO_API UPreviewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UPROPERTY(BlueprintReadOnly, Category="Preview") int32 PoseIndex = 0;
private:
	int32 PreviousState = 0;
	float StateTime = 0.f;
};
