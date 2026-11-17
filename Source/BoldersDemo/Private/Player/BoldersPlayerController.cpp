#include "Player/BoldersPlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "Misc/ScopeExit.h"
#include "Characters/BaseCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/FactionRingComponent.h"
#include "Combat/CharacterCombatComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputMappingContext.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "UI/ClassSelectionWidget.h"
#include "UI/ClassSelectionData.h"
#include "Game/BoldersGameMode.h"

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
	if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (CommandClickAction)
			Input->BindAction(CommandClickAction, ETriggerEvent::Started, this, &ThisClass::HandleCommandClick);
	}
}

void ABoldersPlayerController::HandleCommandClick()
{
	if (IsChoosingClass()) return;
	if (!IsLocalController()) return;
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

void ABoldersPlayerController::UpdateHoveredCharacter(ABaseCharacter* TargetCharacter)
{
	if (HoveredCharacter.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = HoveredCharacter.Get();
	HoveredCharacter = TargetCharacter;
	RefreshOutline(Previous);
	RefreshOutline(TargetCharacter);
}

bool ABoldersPlayerController::CanSelectEnemy(const ABaseCharacter* TargetCharacter) const
{
	return IsValid(TargetCharacter) && TargetCharacter->IsA<AEnemyCharacter>() && TargetCharacter->IsAlive()
		&& IsValid(AssignedCharacter) && AssignedCharacter->IsAlive()
		&& AssignedCharacter->IsHostileTo(TargetCharacter);
}

void ABoldersPlayerController::UpdateSelectedEnemy(ABaseCharacter* TargetCharacter)
{
	if (SelectedEnemy.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = SelectedEnemy.Get();
	SelectedEnemy = TargetCharacter;
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

void ABoldersPlayerController::UpdateSelectedCharacter(ABaseCharacter* TargetCharacter)
{
	if (SelectedCharacter.Get() == TargetCharacter) return;
	ABaseCharacter* Previous = SelectedCharacter.Get();
	SelectedCharacter = TargetCharacter;
	RefreshOutline(Previous);
	RefreshOutline(TargetCharacter);
}

void ABoldersPlayerController::ClearSelection()
{
	if (!IsLocalController()) return;
	UpdateSelectedEnemy(nullptr);
	UpdateSelectedCharacter(nullptr);
}

void ABoldersPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABoldersPlayerController, AssignedCharacter, COND_OwnerOnly);
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

void ABoldersPlayerController::OpenClassSelection()
{
	if (!IsLocalController() || AssignedCharacter || ClassSelectionScreen || !SelectionCatalog || !SelectionWidgetClass) return;
	ClassSelectionScreen=CreateWidget<UClassSelectionWidget>(this,SelectionWidgetClass);
	if (!ClassSelectionScreen) return;
	ClassSelectionScreen->SetIsFocusable(true);
	ClassSelectionScreen->AddToPlayerScreen(100);
	FInputModeUIOnly Mode; Mode.SetWidgetToFocus(ClassSelectionScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
}
void ABoldersPlayerController::CloseClassSelection()
{
	if (!ClassSelectionScreen) return;
	ClassSelectionScreen->RemoveFromParent(); ClassSelectionScreen=nullptr;
	if (IsLocalController())
	{
		FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
	}
}
void ABoldersPlayerController::SubmitClassSelection(FName ClassId)
{
	if (IsLocalController() && !AssignedCharacter) ServerChooseClass(ClassId);
}
void ABoldersPlayerController::ServerChooseClass_Implementation(FName ClassId)
{
	if (AssignedCharacter) return;
	auto* GM=GetWorld()->GetAuthGameMode<ABoldersGameMode>();
	if (!GM || !GM->ChooseStartingClass(this,ClassId))
		ClientClassSelectionFailed(FText::FromString(TEXT("无法生成该职业，请重新选择或稍后重试。")));
}
void ABoldersPlayerController::ClientClassSelectionFailed_Implementation(const FText& Reason)
{
	if (ClassSelectionScreen) ClassSelectionScreen->ShowFailure(Reason);
}
