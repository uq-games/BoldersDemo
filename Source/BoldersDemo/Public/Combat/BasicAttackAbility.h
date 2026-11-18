#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "BasicAttackAbility.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_BasicAttack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_AttackHit);
class ABaseCharacter;
class AWeapon;
struct FAttackModule;

// A single server-authoritative attack command, including its approach.
UCLASS()
class BOLDERSDEMO_API UBasicAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UBasicAttackAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	bool IsTargetInReach() const;
	float GetAttackRange() const;
	const FAttackModule* GetModule() const;
	void OnPositionReady(bool bArrived);
	void StartMontage();
	UFUNCTION() void OnHitEvent(FGameplayEventData Payload);
	UFUNCTION() void OnMontageComplete();
	UFUNCTION() void OnMontageInterrupted();
	void Finish(bool bCancelled);
	UPROPERTY(Transient) TWeakObjectPtr<ABaseCharacter> Attacker;
	UPROPERTY(Transient) TWeakObjectPtr<ABaseCharacter> Target;
	UPROPERTY(Transient) TWeakObjectPtr<AWeapon> Weapon;
	FName ModuleId;
	bool bHitPending = false;
};
