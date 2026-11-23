#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoldersPlayerController.generated.h"

class ABaseCharacter;
class UInputAction;
class UInputMappingContext;
class ULocalPlayer;
class UNiagaraSystem;
class UClassSelectionCatalog;
class UClassSelectionWidget;
UCLASS()
class BOLDERSDEMO_API ABoldersPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	UClassSelectionCatalog* GetSelectionCatalog() const { return SelectionCatalog; }
	bool IsChoosingClass() const { return ClassSelectionScreen != nullptr; }
	UFUNCTION(BlueprintCallable, Category="Class Selection") void SubmitClassSelection(FName ClassId);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void AssignCharacter(ABaseCharacter* TargetCharacter);
	ABaseCharacter* GetAssignedCharacter() const { return AssignedCharacter; }
	int32 GetPlayerSlot() const { return PlayerSlot; }
	void SetPlayerSlot(int32 Slot);
protected:
	UPROPERTY(EditDefaultsOnly, Category="Class Selection") TObjectPtr<UClassSelectionCatalog> SelectionCatalog;
	UPROPERTY(EditDefaultsOnly, Category="Class Selection") TSubclassOf<UClassSelectionWidget> SelectionWidgetClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Class Selection") TObjectPtr<UClassSelectionWidget> ClassSelectionScreen;
	UFUNCTION(Server, Reliable) void ServerChooseClass(FName ClassId);
	UFUNCTION(Client, Reliable) void ClientClassSelectionFailed(const FText& Reason);
	virtual void ReceivedPlayer() override;
	virtual void PlayerTick(float DeltaSeconds) override;
	void UpdateHoveredCharacter(ABaseCharacter* TargetCharacter);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	void HandleCommandClick();
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

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CommandMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> CommandClickAction;
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
	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> HoveredCharacter;
	TWeakObjectPtr<ULocalPlayer> MappingLocalPlayer;
	double LastMoveRequestTime = -1.0;
};
