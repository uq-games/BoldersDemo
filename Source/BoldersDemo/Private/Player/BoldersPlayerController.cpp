#include "Player/BoldersPlayerController.h"

#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Engine/LocalPlayer.h"
#include "Game/VolcanoEncounter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "UI/Combat/BoldersHUD.h"
#include "UObject/ConstructorHelpers.h"

ABoldersPlayerController::ABoldersPlayerController()
{
	static const ConstructorHelpers::FObjectFinder<UInputAction> Movement(
		TEXT("/Game/BoldersDemo/Assets/Input/IA_CameraMove.IA_CameraMove"));
	CombatMovementAction = Movement.Object;
}

void ABoldersPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bDirectCombatControls = GetEncounter() != nullptr;
	if (AssignedCharacter) OnRep_AssignedCharacter();
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
	UpdateCursorTarget(nullptr);
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
	CachedEncounter.Reset();
	UpdateSelectedCharacter(nullptr);
	Super::EndPlay(Reason);
}

void ABoldersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::HandleEscape);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::HandleReadWaySign).bConsumeInput = false;
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::HandleRestartRequest);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ThisClass::RequestBladeDip);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::ToggleAttackRange);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::HandleMoveOnlyClick);
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
		if (CombatMovementAction)
			Input->BindAction(CombatMovementAction, ETriggerEvent::Triggered, this, &ThisClass::HandleCombatMovement);
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
	if (bDirectCombatControls && IsValid(AssignedCharacter))
	{
		auto& Movement = *AssignedCharacter->GetCharacterMovement();
		Movement.MaxWalkSpeed = 410.f;
		Movement.MaxAcceleration = 2800.f;
		Movement.BrakingDecelerationWalking = 2600.f;
		Movement.SetAvoidanceEnabled(false);
	}
	if (IsLocalController() && IsValid(AssignedCharacter)) CloseClassSelection();
	UpdateCursorTarget(nullptr);
	UpdateHoveredCharacter(nullptr);
	UpdateSelectedEnemy(nullptr);
	UpdateSelectedCharacter(IsLocalController() && bDirectCombatControls ? AssignedCharacter.Get() : nullptr);
}

FRotator ABoldersPlayerController::GetFixedCombatViewRotation()
{
	return FRotator(-54.f, -90.f, 0.f);
}

bool ABoldersPlayerController::UsesDirectCombatControls() const
{
	return bDirectCombatControls || (!HasActorBegunPlay() && GetEncounter());
}

AVolcanoEncounter* ABoldersPlayerController::GetEncounter() const
{
	auto* Encounter = CachedEncounter.Get();
	if (!Encounter || Encounter->GetWorld() != GetWorld())
	{
		UWorld* World = GetWorld();
		Encounter = World ? AVolcanoEncounter::Find(World) : nullptr;
		CachedEncounter = Encounter;
	}
	return Encounter;
}

bool ABoldersPlayerController::IsCombatModalOpen() const
{
	const auto* HUD = GetHUD<ABoldersHUD>();
	return HUD && HUD->IsModalOpen();
}

void ABoldersPlayerController::HandleCombatMovement(const FInputActionValue& Value)
{
	if (!bDirectCombatControls || !IsLocalController() || IsChoosingClass() || !IsValid(AssignedCharacter) || GetPawn() != AssignedCharacter
		|| !AssignedCharacter->IsAlive() || AssignedCharacter->IsFrozen() || IsCombatModalOpen()) return;

	const auto* Encounter = GetEncounter();
	if (!Encounter || Encounter->IsFinished()
		|| AssignedCharacter->Combat->GetOrderPhase() == EAttackOrderPhase::Windup) return;

	const FVector2D Move = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
	const FVector Direction = FRotator(0.f, GetFixedCombatViewRotation().Yaw, 0.f).RotateVector(
		FVector(Move.Y, Move.X, 0.f));
	AssignedCharacter->AddMovementInput(Direction);
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
