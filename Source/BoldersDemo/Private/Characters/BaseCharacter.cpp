#include "Characters/BaseCharacter.h"
#include "AI/BoldersAIController.h"
#include "Net/UnrealNetwork.h"
#include "Characters/KayKitEquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	// Small demo arena: relevance must not depend on the stationary server camera.
	bAlwaysRelevant = true;
	AIControllerClass = ABoldersAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCanEverAffectNavigation(false);
	KayKitEquipment = CreateDefaultSubobject<UKayKitEquipmentComponent>(TEXT("KayKitEquipment"));
}

bool ABaseCharacter::RequestMoveToLocation(const FVector& Destination)
{
	if (!HasAuthority() || Destination.ContainsNaN()) return false;
	ABoldersAIController* AI = Cast<ABoldersAIController>(GetController());
	return AI && AI->RequestCharacterMove(Destination);
}

void ABaseCharacter::SetHoverOutline(bool bEnabled)
{
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* Part : Meshes)
	{
		Part->SetCustomDepthStencilValue(200);
		Part->SetRenderCustomDepth(bEnabled);
	}
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ABaseCharacter, PlayerSlot);
}
