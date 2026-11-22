#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

void ABoldersPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (auto* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (CameraMappingContext) Subsystem->AddMappingContext(CameraMappingContext, 0);
			if (CommandMappingContext) Subsystem->AddMappingContext(CommandMappingContext, 1);
			MappingLocalPlayer = LocalPlayer;
		}
	}
	OpenClassSelection();
}

void ABoldersPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	OpenClassSelection();
}

void ABoldersPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseClassSelection();
	UpdateHoveredCharacter(nullptr);
	UpdateSelectedEnemy(nullptr);
	if (ULocalPlayer* LocalPlayer = MappingLocalPlayer.Get())
	{
		if (auto* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (CameraMappingContext) Subsystem->RemoveMappingContext(CameraMappingContext);
			if (CommandMappingContext) Subsystem->RemoveMappingContext(CommandMappingContext);
		}
	}
	MappingLocalPlayer.Reset();
	UpdateSelectedCharacter(nullptr);
	Super::EndPlay(Reason);
}

void ABoldersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ClearSelection);
	const FKey AttackKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AttackKeys); ++Index)
	{
		FInputKeyBinding Binding(FInputChord(AttackKeys[Index]), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Index] { SelectAttackSlot(Index); });
		InputComponent->KeyBindings.Add(MoveTemp(Binding));
	}
	if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (CommandClickAction)
			Input->BindAction(CommandClickAction, ETriggerEvent::Started, this, &ThisClass::HandleCommandClick);
	}
}

void ABoldersPlayerController::AssignCharacter(ABaseCharacter* TargetCharacter)
{
	if (!HasAuthority()) return;
	AssignedCharacter = TargetCharacter;
	OnRep_AssignedCharacter();
	ForceNetUpdate();
}

void ABoldersPlayerController::OnRep_AssignedCharacter()
{
	if (IsLocalController() && IsValid(AssignedCharacter)) CloseClassSelection();
	UpdateHoveredCharacter(nullptr);
	UpdateSelectedEnemy(nullptr);
	UpdateSelectedCharacter(nullptr);
}

void ABoldersPlayerController::SetPlayerSlot(int32 Slot)
{
	if (!HasAuthority() || PlayerSlot == Slot) return;
	PlayerSlot = Slot;
	OnRep_PlayerSlot();
	ForceNetUpdate();
}

void ABoldersPlayerController::OnRep_PlayerSlot()
{
	if (PlayerSlot > 0) OpenClassSelection();
}

void ABoldersPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABoldersPlayerController, AssignedCharacter, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABoldersPlayerController, PlayerSlot, COND_OwnerOnly);
}
