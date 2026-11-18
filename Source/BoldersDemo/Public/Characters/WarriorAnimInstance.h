#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacterAnimInstance.h"
#include "WarriorAnimInstance.generated.h"

UCLASS()
class BOLDERSDEMO_API UWarriorAnimInstance : public UBaseCharacterAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Character")
	bool bUseGreatswordIdle = false;
};
