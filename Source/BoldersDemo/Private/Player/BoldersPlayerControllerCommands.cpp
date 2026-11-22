#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"

void ABoldersPlayerController::HandleCommandClick()
{
	if (IsChoosingClass()) return;
	if (!IsLocalController()) return;
	float MouseX, MouseY;
	if (auto* HUD = GetHUD(); HUD && GetMousePosition(MouseX, MouseY))
	{
		if (const auto* Icon = HUD->GetHitBoxAtCoordinates({MouseX, MouseY}, true))
		{
			if (!bEnableClickEvents) HUD->NotifyHitBoxClick(Icon->GetName());
			return;
		}
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit))
	{
		UpdateSelectedEnemy(nullptr);
		return;
	}
	if (ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(Hit.GetActor()))
	{
		if (TargetCharacter == AssignedCharacter && IsValid(TargetCharacter))
		{
			UpdateSelectedEnemy(nullptr);
			UpdateSelectedCharacter(TargetCharacter);
			UE_LOG(LogTemp, Log, TEXT("WarriorSelection: %s selected %s"), *GetName(), *TargetCharacter->GetName());
		}
		else if (CanSelectEnemy(TargetCharacter))
		{
			UpdateSelectedEnemy(TargetCharacter);
			if (SelectedCharacter.IsValid() && SelectedCharacter->IsAlive())
				ServerRequestAttack(SelectedCharacter.Get(), TargetCharacter);
		}
		else UpdateSelectedEnemy(nullptr);
		return;
	}
	UpdateSelectedEnemy(nullptr);
	if (SelectedCharacter.IsValid() && IsValid(Hit.GetActor()) && Hit.GetActor()->ActorHasTag(TEXT("WalkableGround")))
		ServerRequestMove(SelectedCharacter.Get(), Hit.ImpactPoint);
}

void ABoldersPlayerController::ServerRequestMove_Implementation(ABaseCharacter* TargetCharacter, FVector_NetQuantize Destination)
{
	if (!IsValid(TargetCharacter) || TargetCharacter != AssignedCharacter || Destination.ContainsNaN()) return;
	bool bAccepted = false;
	ON_SCOPE_EXIT { ClientMoveFeedback(bAccepted, Destination); };
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastMoveRequestTime < 0.05) return;
	LastMoveRequestTime = Now;
	if (Destination.GetAbsMax() > 100000.f) return;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WarriorMoveGround), false, TargetCharacter);
	const FVector Target = Destination;
	if (!GetWorld()->LineTraceSingleByChannel(Ground, Target + FVector(0,0,100),
		Target - FVector(0,0,100), ECC_Visibility, Params)) return;
	if (!IsValid(Ground.GetActor()) || !Ground.GetActor()->ActorHasTag(TEXT("WalkableGround"))
		|| Ground.ImpactNormal.Z < TargetCharacter->GetCharacterMovement()->GetWalkableFloorZ()) return;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation NavTarget;
	if (!Nav || !Nav->ProjectPointToNavigation(Ground.ImpactPoint, NavTarget, FVector(50,50,100))) return;
	bAccepted = TargetCharacter->RequestMoveToLocation(NavTarget.Location);
}

void ABoldersPlayerController::ClientMoveFeedback_Implementation(bool bAccepted, FVector Location)
{
	if (auto* Effect = bAccepted ? MoveAcceptedFX.Get() : MoveRejectedFX.Get())
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location + FVector(0,0,3));
}

void ABoldersPlayerController::ServerRequestAttack_Implementation(ABaseCharacter* Attacker, ABaseCharacter* Target)
{
	if (IsValid(Attacker) && Attacker == AssignedCharacter) Attacker->Combat->RequestAttack(Target);
}

void ABoldersPlayerController::SelectAttackSlot(int32 SlotIndex)
{
	if (!IsLocalController() || IsChoosingClass() || !IsValid(AssignedCharacter)) return;
	const UAttackLoadout* Loadout = AssignedCharacter->Combat->AttackLoadout;
	if (Loadout && Loadout->Modules.IsValidIndex(SlotIndex)) ServerEquipAttack(Loadout->Modules[SlotIndex].Id);
}

void ABoldersPlayerController::ServerEquipAttack_Implementation(FName ModuleId)
{
	if (IsValid(AssignedCharacter)) AssignedCharacter->Combat->EquipAttack(ModuleId);
}
