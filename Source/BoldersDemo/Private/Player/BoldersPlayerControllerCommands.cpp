#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Characters/Roles/WarriorCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Game/VolcanoEncounter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	bool IsEncounterFinished(const AVolcanoEncounter* Encounter)
	{
		return Encounter && Encounter->IsFinished();
	}
}

void ABoldersPlayerController::HandleCommandClick()
{
	if (IsChoosingClass() || !IsLocalController()) return;
	float MouseX, MouseY;
	if (auto* HUD = GetHUD(); HUD && GetMousePosition(MouseX, MouseY))
	{
		if (const auto* Icon = HUD->GetHitBoxAtCoordinates({MouseX, MouseY}, true))
		{
			if (!bEnableClickEvents) HUD->NotifyHitBoxClick(Icon->GetName());
			return;
		}
	}
	if (IsCombatModalOpen() || IsEncounterFinished(GetEncounter())) return;
	if (bDirectCombatControls)
	{
		AActor* Target = FindCombatCursorTarget();
		UpdateSelectedEnemy(Cast<ABaseCharacter>(Target));
		if (Target && IsValid(AssignedCharacter) && AssignedCharacter->IsAlive())
			ServerRequestAttack(AssignedCharacter, Target);
		else AttackFeedbackExpiresAt = 0.0;
		return;
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
	if (auto* Hazard = Cast<AVolcanoHazard>(Hit.GetActor()); Hazard && Hazard->CanReceiveAttack())
	{
		if (SelectedCharacter.IsValid() && SelectedCharacter->IsAlive())
			ServerRequestAttack(SelectedCharacter.Get(), Hazard);
		return;
	}
	if (SelectedCharacter.IsValid() && IsValid(Hit.GetActor()) && Hit.GetActor()->ActorHasTag(TEXT("WalkableGround")))
	{
		AttackFeedbackExpiresAt = 0.0;
		ServerRequestMove(SelectedCharacter.Get(), Hit.ImpactPoint);
	}
}

void ABoldersPlayerController::HandleMoveOnlyClick()
{
	const auto* Encounter = GetEncounter();
	if (!IsLocalController() || bDirectCombatControls || IsChoosingClass() || !SelectedCharacter.IsValid()
		|| !Encounter || IsEncounterFinished(Encounter) || IsCombatModalOpen()) return;
	const TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjects{UEngineTypes::ConvertToObjectType(ECC_WorldStatic)};
	FHitResult Hit;
	if (!GetHitResultUnderCursorForObjects(GroundObjects, false, Hit)) return;
	if (AActor* Ground = Hit.GetActor(); IsValid(Ground) && Ground->ActorHasTag(TEXT("WalkableGround")))
	{
		AttackFeedbackExpiresAt = 0.0;
		ServerRequestMove(SelectedCharacter.Get(), Hit.ImpactPoint);
	}
}

void ABoldersPlayerController::ServerRequestMove_Implementation(ABaseCharacter* TargetCharacter, FVector_NetQuantize Destination)
{
	if (bDirectCombatControls || !IsValid(TargetCharacter) || TargetCharacter != AssignedCharacter || Destination.ContainsNaN()) return;
	const auto* Encounter = GetEncounter();
	if (IsEncounterFinished(Encounter)) return;
	bool bAccepted = false;
	ON_SCOPE_EXIT { ClientMoveFeedback(bAccepted, Destination); };
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastMoveRequestTime < 0.05) return;
	LastMoveRequestTime = Now;
	if (Destination.GetAbsMax() > 100000.f) return;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WarriorMoveGround), false, TargetCharacter);
	const FVector Target = Destination;
	const FVector RayStart = Target + FVector(0, 0, 100);
	const FVector RayEnd = Target - FVector(0, 0, 100);
	const bool bHasGround = Encounter
		? GetWorld()->LineTraceSingleByObjectType(Ground, RayStart, RayEnd, FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		: GetWorld()->LineTraceSingleByChannel(Ground, RayStart, RayEnd, ECC_Visibility, Params);
	if (!bHasGround) return;
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

void ABoldersPlayerController::ServerRequestAttack_Implementation(ABaseCharacter* Attacker, AActor* Target)
{
	if (IsValid(Attacker) && Attacker == AssignedCharacter && (!bDirectCombatControls || GetPawn() == Attacker)
		&& !IsEncounterFinished(GetEncounter()))
		ClientAttackOrderFeedback(Attacker->Combat->RequestAttack(Target));
}

void ABoldersPlayerController::ClientAttackOrderFeedback_Implementation(EAttackOrderResult Result)
{
	AttackFeedback = Result;
	AttackFeedbackExpiresAt = GetWorld()->GetTimeSeconds() + (Result == EAttackOrderResult::Buffered ? .48 : 1.2);
}

EAttackOrderResult ABoldersPlayerController::GetRecentAttackOrderResult() const
{
	return GetWorld()->GetTimeSeconds() < AttackFeedbackExpiresAt ? AttackFeedback : EAttackOrderResult::Accepted;
}

void ABoldersPlayerController::ServerRestartVolcano_Implementation()
{
	if (GetPlayerSlot() <= 0) return;
	if (auto* Encounter = GetEncounter()) Encounter->RequestRestart();
}

void ABoldersPlayerController::HandleRestartRequest()
{
	if (IsLocalController() && !IsChoosingClass() && IsEncounterFinished(GetEncounter())) ServerRestartVolcano();
}

void ABoldersPlayerController::SelectAttackSlot(int32 SlotIndex)
{
	if (!IsLocalController() || IsChoosingClass() || !IsValid(AssignedCharacter) || IsCombatModalOpen()
		|| IsEncounterFinished(GetEncounter())) return;
	const UAttackLoadout* Loadout = AssignedCharacter->Combat->AttackLoadout;
	if (Loadout && Loadout->Modules.IsValidIndex(SlotIndex)) ServerEquipAttack(Loadout->Modules[SlotIndex].Id);
}

void ABoldersPlayerController::ServerEquipAttack_Implementation(FName ModuleId)
{
	if (IsValid(AssignedCharacter) && !IsEncounterFinished(GetEncounter())) AssignedCharacter->Combat->EquipAttack(ModuleId);
}

void ABoldersPlayerController::RequestBladeDip()
{
	if (IsLocalController() && !IsChoosingClass() && IsValid(AssignedCharacter) && AssignedCharacter->IsA<AWarriorCharacter>()
		&& !IsCombatModalOpen() && !IsEncounterFinished(GetEncounter()))
		ServerDipBlade();
}

void ABoldersPlayerController::ToggleAttackRange()
{
	if (IsLocalController() && !IsChoosingClass() && !IsCombatModalOpen() && !IsEncounterFinished(GetEncounter()))
		bShowAttackRange = !bShowAttackRange;
}

void ABoldersPlayerController::ServerDipBlade_Implementation()
{
	if (!IsValid(AssignedCharacter) || !AssignedCharacter->IsA<AWarriorCharacter>()
		|| (bDirectCombatControls && GetPawn() != AssignedCharacter)
		|| IsEncounterFinished(GetEncounter())) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastBladeDipRequestTime < .2) return;
	LastBladeDipRequestTime = Now;
	if (!AssignedCharacter->Combat->TryDipBlade()) ClientBladeDipRejected();
}

void ABoldersPlayerController::ClientBladeDipRejected_Implementation()
{
	BladeDipRejectionExpiresAt = GetWorld()->GetTimeSeconds() + 1.3;
}

bool ABoldersPlayerController::HadRecentBladeDipRejection() const
{
	return GetWorld()->GetTimeSeconds() < BladeDipRejectionExpiresAt;
}
