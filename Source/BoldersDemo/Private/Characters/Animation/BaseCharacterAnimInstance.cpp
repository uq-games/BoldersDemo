#include "Characters/Animation/BaseCharacterAnimInstance.h"
#include "Characters/BaseCharacter.h"

void UBaseCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Character = Cast<ABaseCharacter>(TryGetPawnOwner());
	Speed = IsValid(Character) ? Character->GetVelocity().Size2D() : 0.f;
	// Editor previews have no initialized gameplay state and should remain idle.
	bDead = IsValid(Character) && Character->HasActorBegunPlay() && !Character->IsAlive();
}
