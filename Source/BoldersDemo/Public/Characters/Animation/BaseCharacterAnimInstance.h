#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BaseCharacterAnimInstance.generated.h"

// Presentation reads gameplay state; it never owns health or decides damage.
UCLASS()
class BOLDERSDEMO_API UBaseCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Character") float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Character") bool bDead = false;
};
