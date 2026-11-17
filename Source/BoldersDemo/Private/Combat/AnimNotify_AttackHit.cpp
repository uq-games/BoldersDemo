#include "Combat/AnimNotify_AttackHit.h"
#include "Combat/BasicAttackAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_AttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || !IsValid(MeshComp->GetOwner()) || !MeshComp->GetOwner()->HasAuthority()) return;
	FGameplayEventData Payload;
	Payload.Instigator = MeshComp->GetOwner();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(MeshComp->GetOwner(), TAG_Event_AttackHit, Payload);
}
