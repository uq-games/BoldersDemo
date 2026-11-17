#include "AI/AttackPositioningComponent.h"
#include "AIController.h"
#include "Characters/BaseCharacter.h"
#include "Combat/AttackPositionRules.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/CharacterMovementComponent.h"

UAttackPositioningComponent::UAttackPositioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttackPositioningComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Cancel();
	Super::EndPlay(Reason);
}

bool UAttackPositioningComponent::RequestPosition(ABaseCharacter* Target, float WeaponRange,
	UEnvQuery* Query, FOnAttackPositionReady OnReady)
{
	auto* Character = Cast<ABaseCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority()) return false;
	Cancel();
	if (!Character->IsAlive() || !IsValid(Target) || !Target->IsAlive()
		|| !Query || !FMath::IsFinite(WeaponRange) || WeaponRange < 0.f) return false;

	TargetCharacter = Target;
	AttackRange = WeaponRange;
	Completion = MoveTemp(OnReady);
	if (auto* AI = Cast<AAIController>(Character->GetController())) AI->StopMovement();
	if (StartQuery(Query)) return true;
	Cancel();
	return false;
}

bool UAttackPositioningComponent::StartQuery(UEnvQuery* Query)
{
	const auto* Character = CastChecked<ABaseCharacter>(GetOwner());
	const float Reach = AttackPositionRules::GetReach(*Character, *TargetCharacter, AttackRange);
	FEnvQueryRequest Request(Query, GetOwner());
	Request.SetFloatParam(TEXT("InnerRadius"), FMath::Max(Reach - 40.f, 1.f));
	Request.SetFloatParam(TEXT("OuterRadius"), FMath::Max(Reach - 25.f, 1.f));
	QueryID = Request.Execute(EEnvQueryRunMode::SingleResult, this, &ThisClass::OnQueryFinished);
	return QueryID != INDEX_NONE;
}

bool UAttackPositioningComponent::IsCandidateValid(const FVector& NavLocation) const
{
	const auto* Character = Cast<ABaseCharacter>(GetOwner());
	const auto* Target = TargetCharacter.Get();
	return Character && Character->IsAlive() && Target
		&& AttackPositionRules::CanStandAndAttack(*Character, *Target, AttackRange, NavLocation);
}

void UAttackPositioningComponent::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (!Result.IsValid() || Result->QueryID != QueryID) return;
	QueryID = INDEX_NONE;
	if (!Result->IsSuccessful() || !IsCandidateValid(Result->GetItemAsLocation(0)))
	{
		UE_LOG(LogTemp, Log, TEXT("AttackEQS: no attack position"));
		Finish(false);
		return;
	}
	MoveToPosition(Result->GetItemAsLocation(0));
}

void UAttackPositioningComponent::MoveToPosition(const FVector& Destination)
{
	auto* Character = CastChecked<ABaseCharacter>(GetOwner());
	auto* AI = Cast<AAIController>(Character->GetController());
	if (!AI || !AI->GetPathFollowingComponent()) { Finish(false); return; }
	BoundPathFollowing = AI->GetPathFollowingComponent();
	MoveFinishedHandle = BoundPathFollowing->OnRequestFinished.AddUObject(this, &ThisClass::OnMoveFinished);

	FAIMoveRequest Move;
	Move.SetGoalLocation(Destination);
	Move.SetAcceptanceRadius(10.f);
	Move.SetReachTestIncludesAgentRadius(false);
	Move.SetReachTestIncludesGoalRadius(false);
	Move.SetAllowPartialPath(false);
	Move.SetProjectGoalLocation(false);
	const auto Result = AI->MoveTo(Move);
	UE_LOG(LogTemp, Log, TEXT("AttackEQS: %s selected %s move=%d"),
		*Character->GetName(), *Destination.ToCompactString(), int32(Result.Code));
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		MoveID = Result.MoveId;
		Character->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::OnCharacterMoved);
	}
	else Finish(Result.Code == EPathFollowingRequestResult::AlreadyAtGoal);
}

void UAttackPositioningComponent::OnCharacterMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	auto* Character = CastChecked<ABaseCharacter>(GetOwner());
	const auto* PathFollowing = BoundPathFollowing.Get();
	if (!Character->HasAuthority() || !MoveID.IsValid() || !PathFollowing
		|| PathFollowing->GetCurrentRequestId() != MoveID) return;
	const auto* Target = TargetCharacter.Get();
	if (!Character->IsAlive() || !Target || !Target->IsAlive()) { Finish(false); return; }
	if (!Character->GetCharacterMovement()->IsMovingOnGround()) return;

	// Finish in the valid attack region, not only at the sampled EQS coordinate.
	// A small inset keeps the subsequent hit check away from the range boundary.
	const float StopRange = FMath::Max(0.f, AttackRange - 5.f);
	if (AttackPositionRules::CanAttack(*Character, *Target, StopRange, Character->GetActorLocation()))
	{
		UE_LOG(LogTemp, Log, TEXT("AttackApproach: %s ready at %s distance=%.1f"),
			*Character->GetName(), *Character->GetActorLocation().ToCompactString(),
			FVector::Distance(Character->GetActorLocation(), Target->GetActorLocation()));
		// Finish cancels only this request (including its velocity) before notifying Combat.
		Finish(true);
	}
}

void UAttackPositioningComponent::OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID != MoveID) return;
	MoveID = FAIRequestID::InvalidRequest;
	Finish(Result.IsSuccess());
}

void UAttackPositioningComponent::UnbindMovement()
{
	if (auto* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::OnCharacterMoved);
	}
	if (auto* PathFollowing = BoundPathFollowing.Get())
	{
		PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
	}
	MoveFinishedHandle.Reset();
	BoundPathFollowing.Reset();
}

void UAttackPositioningComponent::Cancel()
{
	Completion.Unbind();
	TargetCharacter.Reset();
	const int32 OldQueryID = QueryID;
	QueryID = INDEX_NONE;
	if (OldQueryID != INDEX_NONE)
	{
		if (auto* Manager = UEnvQueryManager::GetCurrent(GetWorld())) Manager->AbortQuery(OldQueryID);
	}

	const FAIRequestID OldMoveID = MoveID;
	MoveID = FAIRequestID::InvalidRequest;
	auto* PathFollowing = BoundPathFollowing.Get();
	UnbindMovement();
	// Abort only our own move; a newer ground-click command must not be cancelled.
	if (PathFollowing && OldMoveID.IsValid())
	{
		PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, OldMoveID);
	}
}

void UAttackPositioningComponent::Finish(bool bArrived)
{
	FOnAttackPositionReady Callback = MoveTemp(Completion);
	Cancel(); // Clear internal state before the caller can start a new request.
	Callback.ExecuteIfBound(bArrived);
}
