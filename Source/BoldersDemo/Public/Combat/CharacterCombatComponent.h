#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "CharacterCombatComponent.generated.h"

class ABaseCharacter;
class AWeapon;
class UCharacterAbilityData;
class UAnimMontage;
class UEnvQuery;

// Owns attack intent, weapon, animation and damage. Positioning is delegated.
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class BOLDERSDEMO_API UCharacterCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCharacterCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool RequestAttack(ABaseCharacter* Target);
	void CancelAttack();
	void GrantBasicAttack();
	AWeapon* GetWeapon() const { return Weapon; }
	UAnimMontage* SelectNextMontage();

	// Keep existing Blueprint configuration on Combat; pass it to the positioning component.
	UPROPERTY(EditDefaultsOnly, Category="Combat|EQS")
	TObjectPtr<UEnvQuery> AttackPositionQuery;
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TSubclassOf<AWeapon> WeaponClass;
	// Character-specific authoring data. The ASC holds the runtime ability spec.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Abilities")
	TObjectPtr<UCharacterAbilityData> AbilityData;

private:
	void SpawnWeapon();

	UPROPERTY(Transient)
	TObjectPtr<ABaseCharacter> Character;
	UPROPERTY(Transient)
	TObjectPtr<AWeapon> Weapon;
	FGameplayAbilitySpecHandle BasicAttackHandle;
	int32 AttackIndex = 0;
};
