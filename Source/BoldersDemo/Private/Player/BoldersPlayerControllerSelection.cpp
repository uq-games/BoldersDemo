#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Characters/Presentation/FactionRingComponent.h"
#include "Characters/Roles/EnemyCharacter.h"

void ABoldersPlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	if (!IsLocalController() || IsChoosingClass()) return;
	if (!CanSelectEnemy(SelectedEnemy.Get())) UpdateSelectedEnemy(nullptr);
	if (SelectedCharacter.IsValid() && !SelectedCharacter->IsAlive()) UpdateSelectedCharacter(nullptr);
	FHitResult Hit;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);
	auto* Candidate = Hit.bBlockingHit ? Cast<ABaseCharacter>(Hit.GetActor()) : nullptr;
	UpdateHoveredCharacter(IsValid(Candidate) && Candidate->IsAlive()
		&& (Candidate == AssignedCharacter || CanSelectEnemy(Candidate)) ? Candidate : nullptr);
}

bool ABoldersPlayerController::CanSelectEnemy(const ABaseCharacter* TargetCharacter) const
{
	return IsValid(TargetCharacter) && TargetCharacter->IsA<AEnemyCharacter>() && TargetCharacter->IsAlive()
		&& IsValid(AssignedCharacter) && AssignedCharacter->IsAlive()
		&& AssignedCharacter->IsHostileTo(TargetCharacter);
}

void ABoldersPlayerController::UpdateHoveredCharacter(ABaseCharacter* TargetCharacter)
{
	if (HoveredCharacter.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = HoveredCharacter.Get();
	HoveredCharacter = TargetCharacter;
	RefreshOutline(Previous);
	RefreshOutline(TargetCharacter);
}

void ABoldersPlayerController::UpdateSelectedEnemy(ABaseCharacter* TargetCharacter)
{
	if (SelectedEnemy.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = SelectedEnemy.Get();
	SelectedEnemy = TargetCharacter;
	RefreshOutline(Previous);
	RefreshOutline(TargetCharacter);
}

void ABoldersPlayerController::UpdateSelectedCharacter(ABaseCharacter* TargetCharacter)
{
	if (SelectedCharacter.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = SelectedCharacter.Get();
	SelectedCharacter = TargetCharacter;
	RefreshOutline(Previous);
	RefreshOutline(TargetCharacter);
}

void ABoldersPlayerController::RefreshOutline(ABaseCharacter* TargetCharacter)
{
	if (!IsLocalController() || !IsValid(TargetCharacter)) return;
	TargetCharacter->SetHoverOutline(TargetCharacter->IsAlive() && HoveredCharacter.Get() == TargetCharacter,
		CanSelectEnemy(TargetCharacter) && SelectedEnemy.Get() == TargetCharacter);
	TargetCharacter->FactionRing->SetSelected(TargetCharacter->IsAlive()
		&& (SelectedCharacter.Get() == TargetCharacter || SelectedEnemy.Get() == TargetCharacter));
}

void ABoldersPlayerController::ClearSelection()
{
	if (!IsLocalController()) return;
	UpdateSelectedEnemy(nullptr);
	UpdateSelectedCharacter(nullptr);
}
