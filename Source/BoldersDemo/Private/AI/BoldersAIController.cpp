#include "AI/BoldersAIController.h"
#include "Navigation/PathFollowingComponent.h"

bool ABoldersAIController::RequestCharacterMove(const FVector& Destination)
{
	if (!HasAuthority() || !GetPawn() || Destination.ContainsNaN()) return false;
	const auto Result = MoveToLocation(Destination, AcceptanceRadius, false, true, false, false, nullptr, false);
	UE_LOG(LogTemp, Log, TEXT("WarriorMove: %s request=%d destination=%s"),
		*GetNameSafe(GetPawn()), static_cast<int32>(Result), *Destination.ToCompactString());
	return Result != EPathFollowingRequestResult::Failed;
}

void ABoldersAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	UE_LOG(LogTemp, Log, TEXT("WarriorMove: %s completed=%s"), *GetNameSafe(GetPawn()), *Result.ToString());
}
