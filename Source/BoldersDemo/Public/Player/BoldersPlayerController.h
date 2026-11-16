#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoldersPlayerController.generated.h"

class ABaseCharacter;
class UInputAction;
class UInputMappingContext;
class ULocalPlayer;
class UNiagaraSystem;
UCLASS()
class BOLDERSDEMO_API ABoldersPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void AssignCharacter(ABaseCharacter* TargetCharacter);
	ABaseCharacter* GetAssignedCharacter() const { return AssignedCharacter; }
protected:
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
	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> HoveredCharacter;
	TWeakObjectPtr<ULocalPlayer> MappingLocalPlayer;
	double LastMoveRequestTime = -1.0;
};
