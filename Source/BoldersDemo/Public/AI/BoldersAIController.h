#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BoldersAIController.generated.h"

UCLASS()
class BOLDERSDEMO_API ABoldersAIController : public AAIController
{
	GENERATED_BODY()
public:
	bool RequestCharacterMove(const FVector& Destination);
	void SetDestinationAcceptanceRadius(float Radius) { AcceptanceRadius = FMath::Max(0.f, Radius); }
protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ClampMin="0.0"))
	float AcceptanceRadius = 50.f;
};
