#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/AttackLoadout.h"
#include "Combat/Data/AttackOrder.h"
#include "GameplayAbilitySpec.h"
#include "CharacterCombatComponent.generated.h"

class ABaseCharacter;
class AWeapon;
class UCharacterAbilityData;
class UAnimMontage;
class UEnvQuery;
class UStaticMeshComponent;
class AVolcanoHazard;

USTRUCT()
struct FBladeCoating
{
	GENERATED_BODY()

	UPROPERTY() EDamageElement Element = EDamageElement::Physical;
	UPROPERTY() float ExpiresAt = 0.f;
	UPROPERTY() float DipReadyAt = 0.f;
};

// Owns attack intent, weapon, animation and damage. Positioning is delegated.
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class BOLDERSDEMO_API UCharacterCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCharacterCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	EAttackOrderResult RequestAttack(AActor* Target, FName ModuleId = NAME_None);
	bool EquipAttack(FName ModuleId);
	const FAttackModule* GetActiveModule() const { return FindModule(ActiveModuleId); }
	const FAttackModule* FindModule(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Combat|Abilities")
	FName GetActiveModuleId() const { return ActiveModuleId; }
	UFUNCTION(BlueprintPure, Category="Combat|Abilities")
	float GetCooldownRemaining(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Combat|Abilities")
	EAttackOrderPhase GetOrderPhase() const;
	void SetOrderPhase(EAttackOrderPhase Phase);
	void CommitAttackCooldown();
	void CancelAttack();
	void GrantBasicAttack();
	bool TryDipBlade();
	void ClearBladeCoating();
	void SpendBladeCoating(EDamageElement Element, const FVector& Contact);
	const AVolcanoHazard* FindBladeDipSource(EDamageElement& Element, FVector& Contact) const;
	UFUNCTION(BlueprintPure, Category="Combat|Equipment")
	EDamageElement GetBladeCoating() const;
	EDamageElement GetEffectiveAttackElement(const FAttackModule* Module) const;
	void ReportAttackOrderResult(EAttackOrderResult Result) const;
	UFUNCTION(BlueprintPure, Category="Combat|Equipment")
	float GetBladeCoatingRemaining() const;
	UFUNCTION(BlueprintPure, Category="Combat|Equipment")
	float GetBladeDipCooldown() const;
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
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	friend class FVolcanoHazardResponseTest;
#endif
	void SpawnWeapon();
	void RunBufferedAttack();
	UFUNCTION() void OnRep_Equipment();
	UFUNCTION() void OnRep_BladeCoating();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBladeCoatingEffect(EDamageElement Element, FVector_NetQuantize Contact, bool bDip);
	UPROPERTY(ReplicatedUsing=OnRep_BladeCoating) FBladeCoating BladeCoating;
	UPROPERTY(ReplicatedUsing=OnRep_Equipment) FName ActiveModuleId;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Offhand;
	// Indexed by the immutable loadout. Server timestamps so HUD countdowns agree with validation.
	UPROPERTY(Replicated) TArray<float> ReadyTimes;
	UPROPERTY(Replicated) FAttackOrderPresentation OrderPresentation;
	float GetCombatTime() const;

	UPROPERTY(Transient)
	TObjectPtr<ABaseCharacter> Character;
	UPROPERTY(Transient, ReplicatedUsing=OnRep_Equipment)
	TObjectPtr<AWeapon> Weapon;
	FGameplayAbilitySpecHandle BasicAttackHandle;
	TWeakObjectPtr<AActor> OrderedTarget;
	TWeakObjectPtr<AActor> BufferedTarget;
	FTimerHandle BufferedAttackTimer;
	FTimerHandle BladeCoatingTimer;
	int32 AttackIndex = 0;
};
