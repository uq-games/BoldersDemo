#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UStaticMeshComponent;
class UAnimMontage;
class AAttackProjectile;
class AWarriorBladeTrail;
class UAnimNotifyState_WeaponTrail;
class USkeletalMeshComponent;

// A weapon supplies appearance and attack configuration; the character executes combat.
UCLASS()
class BOLDERSDEMO_API AWeapon : public AActor
{
	GENERATED_BODY()
public:
	AWeapon();
	void StartBladeTrail(USkeletalMeshComponent& Pose, const UAnimNotifyState_WeaponTrail& Style);
	void StopBladeTrail(const UAnimNotifyState_WeaponTrail& Style);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	FName AttachSocket = TEXT("Weapon_R");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0"))
	float Damage = 20.f;
	// Distance between capsule surfaces, not actor centres.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0"))
	float AttackRange = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;
	// Null means melee; configured classes launch a projectile at AttackHit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ranged")
	TSubclassOf<AAttackProjectile> ProjectileClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ranged")
	bool bUseWeaponReleasePoint = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ranged", meta=(EditCondition="bUseWeaponReleasePoint"))
	FVector WeaponReleasePoint = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ranged", meta=(EditCondition="!bUseWeaponReleasePoint"))
	FVector ProjectileSpawnOffset = FVector(45.f, 0.f, 20.f);

private:
	TWeakObjectPtr<AWarriorBladeTrail> BladeTrail;
};
