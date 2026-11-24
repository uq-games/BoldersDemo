#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/BoldersGameMode.h"
#include "UI/ClassSelection/ClassSelectionWidget.h"
#include "Widgets/SViewport.h"

void ABoldersPlayerController::OpenClassSelection()
{
	if (!GetLocalPlayer() || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || PlayerSlot <= 0
		|| AssignedCharacter || ClassSelectionScreen || !SelectionCatalog || !SelectionWidgetClass) return;
	ClassSelectionScreen = CreateWidget<UClassSelectionWidget>(this, SelectionWidgetClass);
	if (!ClassSelectionScreen) return;
	ClassSelectionScreen->SetIsFocusable(true);
	ClassSelectionScreen->AddToPlayerScreen(100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(ClassSelectionScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ABoldersPlayerController::CloseClassSelection()
{
	if (!ClassSelectionScreen) return;
	ClassSelectionScreen->RemoveFromParent();
	ClassSelectionScreen = nullptr;
	if (IsLocalController())
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (UsesDirectCombatControls() && IsValid(AssignedCharacter))
			if (auto* Viewport = GetWorld()->GetGameViewport())
				Mode.SetWidgetToFocus(Viewport->GetGameViewportWidget());
		SetInputMode(Mode);
	}
}

void ABoldersPlayerController::SubmitClassSelection(FName ClassId)
{
	if (IsLocalController() && !AssignedCharacter) ServerChooseClass(ClassId);
}

void ABoldersPlayerController::ServerChooseClass_Implementation(FName ClassId)
{
	if (AssignedCharacter) return;
	ABoldersGameMode* GameMode = GetWorld()->GetAuthGameMode<ABoldersGameMode>();
	if (!GameMode || !GameMode->ChooseStartingClass(this, ClassId))
		ClientClassSelectionFailed(FText::FromString(TEXT("无法生成该职业，请重新选择或稍后重试。")));
}

void ABoldersPlayerController::ClientClassSelectionFailed_Implementation(const FText& Reason)
{
	if (ClassSelectionScreen) ClassSelectionScreen->ShowFailure(Reason);
}
