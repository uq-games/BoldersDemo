#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class UKayKitEquipmentComponent;
UCLASS()
class BOLDERSDEMO_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ABaseCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character")
	int32 PlayerSlot = 0;
	// Local presentation only; does not change selection or gameplay authority.
	void SetHoverOutline(bool bEnabled);
	// Accepted does not mean arrived. Only the server may execute this request.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character|Movement")
	bool RequestMoveToLocation(const FVector& Destination);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character|Equipment")
	TObjectPtr<UKayKitEquipmentComponent> KayKitEquipment;
};
