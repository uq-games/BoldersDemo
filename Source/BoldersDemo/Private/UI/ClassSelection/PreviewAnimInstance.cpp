#include "UI/ClassSelection/PreviewAnimInstance.h"
#include "UI/ClassSelection/CharacterPreview.h"
#include "UI/ClassSelection/ClassSelectionData.h"
#include "Animation/AnimSequence.h"

void UPreviewAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Preview = Cast<ACharacterPreview>(GetOwningActor());
	if (!Preview || !Preview->Definition) return;
	const int32 State = Preview->GetDisplayState();
	if (PreviousState != State) { PreviousState = State; StateTime = 0.f; }
	StateTime += DeltaSeconds;
	const auto* Action = State == 1 ? Preview->Definition->HoverAnimation.Get() : Preview->Definition->SelectedAnimation.Get();
	// One-shot gestures return to idle; UI selection remains independent of the pose.
	PoseIndex = State != 0 && Action && StateTime < FMath::Max(.1f, Action->GetPlayLength() - .25f) ? State : 0;
}
