#include "Combat/Presentation/AnimNotifyState_WeaponTrail.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Equipment/Weapon.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	AWeapon* ResolveWeapon(const USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || Mesh->GetNetMode() == NM_DedicatedServer) return nullptr;
		const auto* Character = Cast<ABaseCharacter>(Mesh->GetOwner());
		return Character && Character->GetMesh() == Mesh && Character->Combat
			? Character->Combat->GetWeapon() : nullptr;
	}
}

void UAnimNotifyState_WeaponTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (AWeapon* Weapon = ResolveWeapon(MeshComp))
		Weapon->StartBladeTrail(*MeshComp, *this);
}

void UAnimNotifyState_WeaponTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (AWeapon* Weapon = ResolveWeapon(MeshComp))
		Weapon->StopBladeTrail(*this);
}
