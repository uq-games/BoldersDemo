#include "Characters/StatueBoss/StatueBossAnimInstance.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"

// UObject reads and time changes happen on the game thread. Evaluation only reads the copied samples.
struct FStatueBossAnimProxy final : FAnimInstanceProxy
{
	explicit FStatueBossAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto& State = *CastChecked<UStatueBossAnimInstance>(Instance);
		CurrentSequence = State.CurrentSequence;
		PreviousSequence = State.PreviousSequence;
		CurrentTime = State.CurrentTime;
		PreviousTime = State.PreviousTime;
		CurrentWeight = State.BlendDuration > 0.f
			? FMath::SmoothStep(0.f, 1.f, 1.f - State.BlendRemaining / State.BlendDuration) : 1.f;
	}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext&) override {}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (!CurrentSequence)
		{
			Output.ResetToRefPose();
			return true;
		}
		FAnimationPoseData CurrentPose(Output);
		CurrentSequence->GetAnimationPose(CurrentPose, FAnimExtractContext(static_cast<double>(CurrentTime)));
		if (PreviousSequence && CurrentWeight < 1.f)
		{
			FPoseContext Previous(Output);
			FAnimationPoseData PreviousPose(Previous);
			PreviousSequence->GetAnimationPose(PreviousPose, FAnimExtractContext(static_cast<double>(PreviousTime)));
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(CurrentPose, PreviousPose, CurrentWeight);
		}
		return true;
	}

	const UAnimSequence* CurrentSequence = nullptr;
	const UAnimSequence* PreviousSequence = nullptr;
	float CurrentTime = 0.f;
	float PreviousTime = 0.f;
	float CurrentWeight = 1.f;
};

void UStatueBossAnimInstance::StartPose(UAnimSequence* Sequence, float Time, float BlendSeconds)
{
	PreviousSequence = CurrentSequence;
	PreviousTime = CurrentTime;
	CurrentSequence = Sequence;
	CurrentTime = Time;
	BlendDuration = PreviousSequence ? BlendSeconds : 0.f;
	BlendRemaining = BlendDuration;
}

void UStatueBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	BlendRemaining = FMath::Max(0.f, BlendRemaining - DeltaSeconds);
	if (BlendRemaining == 0.f) PreviousSequence = nullptr;
}

FAnimInstanceProxy* UStatueBossAnimInstance::CreateAnimInstanceProxy()
{
	return new FStatueBossAnimProxy(this);
}

void UStatueBossAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete static_cast<FStatueBossAnimProxy*>(Proxy);
}
