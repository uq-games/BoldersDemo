#pragma once

#include "Animation/AnimInstance.h"
#include "StatueBossAnimInstance.generated.h"

class UAnimSequence;
struct FStatueBossAnimProxy;

UCLASS()
class UStatueBossAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void StartPose(UAnimSequence* Sequence, float Time, float BlendSeconds);
	void SetSampleTime(float Time) { CurrentTime = Time; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;

private:
	friend struct FStatueBossAnimProxy;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentSequence;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> PreviousSequence;
	float CurrentTime = 0.f;
	float PreviousTime = 0.f;
	float BlendRemaining = 0.f;
	float BlendDuration = 0.f;
};
