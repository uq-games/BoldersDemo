#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/AttackLoadout.h"
#include "GameplayAbilitySpec.h"
#include "CharacterCombatComponent.generated.h"

class ABaseCharacter;
class AWeapon;
class UCharacterAbilityData;
class UAnimMontage;
class UEnvQuery;
class UStaticMeshComponent;

// Owns attack intent, weapon, animation and damage. Positioning is delegated.
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class BOLDERSDEMO_API UCharacterCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCharacterCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool RequestAttack(ABaseCharacter* Target, FName ModuleId = NAME_None);
	bool EquipAttack(FName ModuleId);
	const FAttackModule* GetActiveModule() const { return FindModule(ActiveModuleId); }
	const FAttackModule* FindModule(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Combat|Abilities")
	FName GetActiveModuleId() const { return ActiveModuleId; }
	UFUNCTION(BlueprintPure, Category="Combat|Abilities")
	float GetCooldownRemaining(FName Id) const;
	void CommitAttackCooldown();
	void CancelAttack();
	void GrantBasicAttack();
	UFUNCTION(BlueprintPure, Category="Combat|Equipment")
	AWeapon* GetWeapon() const { return Weapon; }
	UAnimMontage* SelectNextMontage();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Keep existing Blueprint configuration on Combat; pass it to the positioning component.
	UPROPERTY(EditDefaultsOnly, Category="Combat|EQS")
	TObjectPtr<UEnvQuery> AttackPositionQuery;
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TSubclassOf<AWeapon> WeaponClass;
	// Character-specific authoring data. The ASC holds the runtime ability spec.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Abilities")
	TObjectPtr<UCharacterAbilityData> AbilityData;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Abilities")
	TObjectPtr<UAttackLoadout> AttackLoadout;

private:
	void SpawnWeapon();
	UFUNCTION() void OnRep_Equipment();
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) FName ActiveModuleId;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Offhand;
	// Indexed by the immutable loadout. Server timestamps so HUD countdowns agree with validation.
	UPROPERTY(Replicated) TArray<float> ReadyTimes;
	float GetCombatTime() const;

	UPROPERTY(Transient)
	TObjectPtr<ABaseCharacter> Character;
	UPROPERTY(Transient, ReplicatedUsing=OnRep_Equipment)
	TObjectPtr<AWeapon> Weapon;
	FGameplayAbilitySpecHandle BasicAttackHandle;
	int32 AttackIndex = 0;
};
