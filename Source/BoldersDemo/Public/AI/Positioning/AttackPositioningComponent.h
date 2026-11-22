#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "AttackPositioningComponent.generated.h"

class ABaseCharacter;
class UEnvQuery;
struct FEnvQueryResult;

DECLARE_DELEGATE_OneParam(FOnAttackPositionReady, bool /*bArrived*/);

// Server-only workflow: find an attack position, move there, report completion.
UCLASS(ClassGroup=(AI))
class BOLDERSDEMO_API UAttackPositioningComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAttackPositioningComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool RequestPosition(ABaseCharacter* Target, float WeaponRange, UEnvQuery* Query,
		FOnAttackPositionReady OnReady);
	void Cancel();
	ABaseCharacter* GetTarget() const { return TargetCharacter.Get(); }
	bool IsCandidateValid(const FVector& NavLocation) const;

private:
	bool StartQuery(UEnvQuery* Query);
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);
	void MoveToPosition(const FVector& Destination);
	void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	UFUNCTION()
	void OnCharacterMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void Finish(bool bArrived);
	void UnbindMovement();

	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseCharacter> TargetCharacter;
	// Remember the exact component we subscribed to, even if the controller changes.
	UPROPERTY(Transient)
	TWeakObjectPtr<UPathFollowingComponent> BoundPathFollowing;
	float AttackRange = 0.f;
	int32 QueryID = INDEX_NONE;
	FAIRequestID MoveID = FAIRequestID::InvalidRequest;
	FDelegateHandle MoveFinishedHandle;
	FOnAttackPositionReady Completion;
};
