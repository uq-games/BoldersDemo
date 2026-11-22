#include "Characters/Roles/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

AEnemyCharacter::AEnemyCharacter()
{
	Faction = ECharacterFaction::Enemy;
}

#if WITH_EDITOR
void AEnemyCharacter::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;

	// This editor flag is transient: reapply it when loading or placing the actor.
	// Evaluate once as well so the pose is correct even before the viewport ticks.
	USkeletalMeshComponent* PreviewMesh = GetMesh();
	if (PreviewMesh && PreviewMesh->IsRegistered())
	{
		PreviewMesh->SetUpdateAnimationInEditor(true);
		PreviewMesh->TickAnimation(0.f, false);
		PreviewMesh->RefreshBoneTransforms();
	}
}
#endif
