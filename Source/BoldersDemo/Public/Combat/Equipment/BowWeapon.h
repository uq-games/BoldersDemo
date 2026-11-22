#pragma once
#include "CoreMinimal.h"
#include "Combat/Equipment/Weapon.h"
#include "BowWeapon.generated.h"

// Cosmetic bowstring follows the authored BowDraw animation curve on every machine.
UCLASS()
class BOLDERSDEMO_API ABowWeapon : public AWeapon
{
	GENERATED_BODY()
public:
	ABowWeapon();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> UpperString;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> LowerString;
};
