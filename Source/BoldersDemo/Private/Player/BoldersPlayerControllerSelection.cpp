#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Characters/Presentation/FactionRingComponent.h"
#include "Characters/Roles/EnemyCharacter.h"
#include "Characters/StatueBoss/StatueBossCharacter.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "GameFramework/HUD.h"
#include "UI/Combat/BoldersHUD.h"

void ABoldersPlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	if (!IsLocalController() || IsChoosingClass()) return;
	if (!CanSelectEnemy(SelectedEnemy.Get())) UpdateSelectedEnemy(nullptr);
	if (SelectedCharacter.IsValid() && !SelectedCharacter->IsAlive()) UpdateSelectedCharacter(nullptr);
	if (bDirectCombatControls)
	{
		AActor* Target = FindCombatCursorTarget();
		UpdateHoveredCharacter(Cast<ABaseCharacter>(Target));
		UpdateCursorTarget(Target);
		CurrentMouseCursor = Target ? EMouseCursor::Crosshairs : EMouseCursor::Default;
		return;
	}
	FHitResult Hit;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);
	auto* Candidate = Hit.bBlockingHit ? Cast<ABaseCharacter>(Hit.GetActor()) : nullptr;
	UpdateHoveredCharacter(IsValid(Candidate) && Candidate->IsAlive()
		&& (Candidate == AssignedCharacter || CanSelectEnemy(Candidate)) ? Candidate : nullptr);
	if (auto* Hazard = Cast<AVolcanoHazard>(Hit.GetActor()); Hazard && Hazard->CanReceiveAttack()) UpdateCursorTarget(Hazard);
	else UpdateCursorTarget(CanSelectEnemy(Candidate) ? Candidate : nullptr);
}

AActor* ABoldersPlayerController::FindCombatCursorTarget() const
{
	FVector2D Cursor;
	if (!IsValid(AssignedCharacter) || !AssignedCharacter->IsAlive()
		|| !GetMousePosition(Cursor.X, Cursor.Y)) return nullptr;
	if (const auto* HUD = GetHUD(); HUD && HUD->GetHitBoxAtCoordinates(Cursor, true)) return nullptr;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VolcanoCombatCursor), false, AssignedCharacter);
	const auto Trace = [this, &Params](const FVector2D& Pixel) -> AActor*
	{
		FHitResult Hit;
		if (!GetHitResultAtScreenPosition(Pixel, ECC_Visibility, Params, Hit)) return nullptr;
		AActor* Actor = Hit.GetActor();
		if (CanSelectEnemy(Cast<ABaseCharacter>(Actor))) return Actor;
		const auto* Hazard = Cast<AVolcanoHazard>(Actor);
		return Hazard && Hazard->IsEncounterActive() ? Actor : nullptr;
	};

	if (AActor* Exact = Trace(Cursor)) return Exact;
	// Four short rays keep a near-edge click on the visible target, respecting walls on each ray.
	for (const FVector2D Offset : {FVector2D(6, 0), FVector2D(-6, 0), FVector2D(0, 6), FVector2D(0, -6)})
		if (AActor* Target = Trace(Cursor + Offset)) return Target;
	return nullptr;
}

void ABoldersPlayerController::UpdateCursorTarget(AActor* Target)
{
	if (CursorTarget.Get() == Target) return;
	if (auto* Hazard = Cast<AVolcanoHazard>(CursorTarget.Get())) Hazard->SetLocalFocus(false);
	CursorTarget = Target;
	if (auto* Hazard = Cast<AVolcanoHazard>(CursorTarget.Get())) Hazard->SetLocalFocus(SelectedCharacter.IsValid());
}

bool ABoldersPlayerController::CanSelectEnemy(const ABaseCharacter* TargetCharacter) const
{
	if (const auto* Boss = Cast<AStatueBossCharacter>(TargetCharacter); Boss && !Boss->IsAwakened()) return false;
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
	if (auto* Hazard = Cast<AVolcanoHazard>(CursorTarget.Get())) Hazard->SetLocalFocus(IsValid(TargetCharacter));
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

void ABoldersPlayerController::HandleEscape()
{
	if (auto* HUD = GetHUD<ABoldersHUD>(); HUD && (HUD->CloseIslandMap() || HUD->CloseWaySign())) return;
	ClearSelection();
}

void ABoldersPlayerController::HandleReadWaySign()
{
	if (IsChoosingClass() || !IsLocalController()) return;
	if (auto* HUD = GetHUD<ABoldersHUD>()) HUD->ReadNearestWaySign();
}

void ABoldersPlayerController::ClearSelection()
{
	if (!IsLocalController()) return;
	UpdateSelectedEnemy(nullptr);
	if (!bDirectCombatControls) UpdateSelectedCharacter(nullptr);
}
