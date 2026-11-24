#pragma once

#include "CoreMinimal.h"
#include "AttackOrder.generated.h"

UENUM()
enum class EAttackOrderResult : uint8
{
	Accepted, Buffered, CoolingDown, Frozen, InvalidTarget, Unavailable, OutOfRange, Obstructed, Ineffective
};

UENUM(BlueprintType)
enum class EAttackOrderPhase : uint8
{
	Idle, Approaching, Windup, Released, NoApproach, LostTarget, Obstructed, Ineffective
};

USTRUCT()
struct FAttackOrderPresentation
{
	GENERATED_BODY()

	UPROPERTY() EAttackOrderPhase Phase = EAttackOrderPhase::Idle;
	UPROPERTY() float ChangedAt = 0.f;
};
