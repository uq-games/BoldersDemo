#include "Player/BoldersPlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "Misc/ScopeExit.h"
#include "Characters/BaseCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputMappingContext.h"
#include "NavigationSystem.h"
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
	if (ULocalPlayer* LocalPlayer = MappingLocalPlayer.Get())
	{
		if (auto* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (CameraMappingContext) Subsystem->RemoveMappingContext(CameraMappingContext);
			if (CommandMappingContext) Subsystem->RemoveMappingContext(CommandMappingContext);
		}
	}
	MappingLocalPlayer.Reset();
	SelectedCharacter.Reset();
	Super::EndPlay(Reason);
}

void ABoldersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (CommandClickAction)
			Input->BindAction(CommandClickAction, ETriggerEvent::Started, this, &ThisClass::HandleCommandClick);
	}
}

void ABoldersPlayerController::HandleCommandClick()
{
	if (!IsLocalController() || IsChoosingClass()) return;
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return;
	if (ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(Hit.GetActor()))
	{
		if (TargetCharacter == AssignedCharacter && IsValid(TargetCharacter))
		{
			SelectedCharacter = TargetCharacter;
			UE_LOG(LogTemp, Log, TEXT("WarriorSelection: %s selected %s"), *GetName(), *TargetCharacter->GetName());
		}
		return;
	}
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
	SelectedCharacter.Reset();
}

void ABoldersPlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	if (!IsLocalController() || IsChoosingClass()) return;
	FHitResult Hit;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);
	auto* Candidate = Hit.bBlockingHit ? Cast<ABaseCharacter>(Hit.GetActor()) : nullptr;
	UpdateHoveredCharacter(IsValid(Candidate) && Candidate == AssignedCharacter ? Candidate : nullptr);
}

void ABoldersPlayerController::UpdateHoveredCharacter(ABaseCharacter* TargetCharacter)
{
	if (HoveredCharacter.Get() == TargetCharacter) return;
	if (HoveredCharacter.IsValid()) HoveredCharacter->SetHoverOutline(false);
	HoveredCharacter = TargetCharacter;
	if (HoveredCharacter.IsValid()) HoveredCharacter->SetHoverOutline(true);
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

void ABoldersPlayerController::ClientMoveFeedback_Implementation(bool bAccepted, FVector Location)
{
	if (auto* Effect = bAccepted ? MoveAcceptedFX.Get() : MoveRejectedFX.Get())
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location + FVector(0,0,3));
}
