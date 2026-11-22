#include "Characters/Animation/WarriorAnimInstance.h"
#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"

void UWarriorAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Character = Cast<ABaseCharacter>(TryGetPawnOwner());
	bUseGreatswordIdle = Character && Speed < 5.f
		&& Character->Combat->GetActiveModuleId() == TEXT("Greatsword");
}
