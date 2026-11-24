#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/AttackOrder.h"
#include "GameFramework/PlayerController.h"
#include "BoldersPlayerController.generated.h"

class ABaseCharacter;
class AVolcanoEncounter;
class UInputAction;
class UInputMappingContext;
class ULocalPlayer;
class UNiagaraSystem;
class UClassSelectionCatalog;
class UClassSelectionWidget;
struct FInputActionValue;
UCLASS()
class BOLDERSDEMO_API ABoldersPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ABoldersPlayerController();
	UClassSelectionCatalog* GetSelectionCatalog() const { return SelectionCatalog; }
	bool IsChoosingClass() const { return ClassSelectionScreen != nullptr; }
	UFUNCTION(BlueprintCallable, Category="Class Selection") void SubmitClassSelection(FName ClassId);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void AssignCharacter(ABaseCharacter* TargetCharacter);
	ABaseCharacter* GetAssignedCharacter() const { return AssignedCharacter; }
	AVolcanoEncounter* GetEncounter() const;
	void HandleRestartRequest();
	int32 GetPlayerSlot() const { return PlayerSlot; }
	AActor* GetCursorTarget() const { return CursorTarget.Get(); }
	ABaseCharacter* GetSelectedEnemy() const { return SelectedEnemy.Get(); }
	bool HasSelectedCharacter() const { return SelectedCharacter.IsValid(); }
	bool UsesDirectCombatControls() const;
	static FRotator GetFixedCombatViewRotation();
	EAttackOrderResult GetRecentAttackOrderResult() const;
	UFUNCTION(Client, Unreliable) void ClientAttackOrderFeedback(EAttackOrderResult Result);
	bool IsAttackRangeVisible() const { return bShowAttackRange; }
	bool HadRecentBladeDipRejection() const;
	void SetPlayerSlot(int32 Slot);
	UFUNCTION(BlueprintCallable, Category="Selection")
	void ClearSelection();
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SelectAttackSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="Combat")
	void RequestBladeDip();
	UFUNCTION(BlueprintCallable, Category="Combat")
	void ToggleAttackRange();
protected:
	UPROPERTY(EditDefaultsOnly, Category="Class Selection") TObjectPtr<UClassSelectionCatalog> SelectionCatalog;
	UPROPERTY(EditDefaultsOnly, Category="Class Selection") TSubclassOf<UClassSelectionWidget> SelectionWidgetClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Class Selection") TObjectPtr<UClassSelectionWidget> ClassSelectionScreen;
	UFUNCTION(Server, Reliable) void ServerChooseClass(FName ClassId);
	UFUNCTION(Client, Reliable) void ClientClassSelectionFailed(const FText& Reason);
	virtual void PlayerTick(float DeltaSeconds) override;
	void UpdateHoveredCharacter(ABaseCharacter* TargetCharacter);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void ReceivedPlayer() override;
	virtual void SetupInputComponent() override;
	void HandleCommandClick();
	void HandleMoveOnlyClick();
	UFUNCTION(Client, Reliable)
	void ClientMoveFeedback(bool bAccepted, FVector Location);
	UPROPERTY(EditDefaultsOnly, Category="Presentation")
	TObjectPtr<UNiagaraSystem> MoveAcceptedFX;
	UPROPERTY(EditDefaultsOnly, Category="Presentation")
	TObjectPtr<UNiagaraSystem> MoveRejectedFX;
	UFUNCTION(Server, Reliable)
	void ServerRequestMove(ABaseCharacter* TargetCharacter, FVector_NetQuantize Destination);
	UFUNCTION()
	void OnRep_AssignedCharacter();
	UFUNCTION(Server, Reliable) void ServerRequestAttack(ABaseCharacter* Attacker, AActor* Target);
	UFUNCTION(Server, Reliable) void ServerRestartVolcano();
	UFUNCTION(Server, Reliable) void ServerEquipAttack(FName ModuleId);
	UFUNCTION(Server, Reliable) void ServerDipBlade();
	UFUNCTION(Client, Unreliable) void ClientBladeDipRejected();

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CommandMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CommandClickAction;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CombatMovementAction;
	// Server authorization record; deliberately separate from Actor::Owner.
	UPROPERTY(Transient, ReplicatedUsing=OnRep_AssignedCharacter, BlueprintReadOnly, Category="Character")
	TObjectPtr<ABaseCharacter> AssignedCharacter;
	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> SelectedCharacter;
private:
	UFUNCTION() void OnRep_PlayerSlot();
	UPROPERTY(Transient, ReplicatedUsing=OnRep_PlayerSlot, BlueprintReadOnly, Category="Class Selection", meta=(AllowPrivateAccess="true"))
	int32 PlayerSlot = 0;
	void OpenClassSelection();
	void CloseClassSelection();
	friend class FMonsterOutlineStateTest;
	friend class FFactionRingTest;
	void UpdateSelectedCharacter(ABaseCharacter* TargetCharacter);
	void HandleEscape();
	void HandleReadWaySign();
	bool CanSelectEnemy(const ABaseCharacter* TargetCharacter) const;
	void UpdateSelectedEnemy(ABaseCharacter* TargetCharacter);
	void UpdateCursorTarget(AActor* Target);
	void RefreshOutline(ABaseCharacter* TargetCharacter);
	void HandleCombatMovement(const FInputActionValue& Value);
	bool IsCombatModalOpen() const;
	mutable TWeakObjectPtr<AVolcanoEncounter> CachedEncounter;
	AActor* FindCombatCursorTarget() const;
	// Local visual selection, independent of the commanded player character.
	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> SelectedEnemy;
	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> HoveredCharacter;
	TWeakObjectPtr<AActor> CursorTarget;
	TWeakObjectPtr<ULocalPlayer> MappingLocalPlayer;
	bool bDirectCombatControls = false;
	bool bShowAttackRange = true;
	double BladeDipRejectionExpiresAt = 0.0;
	double LastBladeDipRequestTime = -1.0;
	double LastMoveRequestTime = -1.0;
	double AttackFeedbackExpiresAt = 0.0;
	EAttackOrderResult AttackFeedback = EAttackOrderResult::Accepted;
};
