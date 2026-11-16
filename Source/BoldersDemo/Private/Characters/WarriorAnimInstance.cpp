#include "Characters/WarriorAnimInstance.h"
#include "GameFramework/Pawn.h"
void UWarriorAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	Speed = IsValid(Pawn) ? Pawn->GetVelocity().Size2D() : 0.f;
}
