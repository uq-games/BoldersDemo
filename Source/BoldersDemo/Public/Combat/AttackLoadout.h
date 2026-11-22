#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/ElementalReactionTypes.h"
#include "AttackLoadout.generated.h"

class AAttackProjectile;
class UAnimMontage;
class UMaterialInterface;
class UStaticMesh;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EDamageElement : uint8
{
	Physical, Fire, Ice, Water, Lightning
};

UENUM(BlueprintType)
enum class EAttackPattern : uint8
{
	SingleMelee, Cleave, Projectile, Ricochet, Blast, Freeze, Wave, SkyStrike, IceLance
};

UENUM()
enum class EAttackVisualStage : uint8
{
	Cast, Impact, SkyImpact, Ricochet
};

USTRUCT(BlueprintType)
struct FAttackModule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack") EDamageElement Element = EDamageElement::Physical;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack") EAttackPattern Pattern = EAttackPattern::Projectile;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0")) float Damage = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0")) float Range = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0")) float Radius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0")) float Speed = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0")) float Cooldown = 1.f;
	// Peak horizontal velocity imparted by this attack's pressure at its center (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Numbers", meta=(ClampMin="0", ClampMax="1200"))
	float ImpactPush = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimMontage>> Montages;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UStaticMesh> WeaponMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") FVector WeaponGripOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UStaticMesh> OffhandMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") FName OffhandSocket = TEXT("Shield_L");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile") TSubclassOf<AAttackProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UStaticMesh> ProjectileMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UMaterialInterface> ProjectileMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> Trail;
	// Visual-only pieces with their own material slots, sharing the projectile's local origin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TArray<TObjectPtr<UStaticMesh>> ProjectileLayers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMesh> FlightParticleMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> Impact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> GroundMark;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> Cast;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> RicochetLink;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMesh> ImpactShardMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> ImpactShardMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UNiagaraSystem> ImpactParticles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UMaterialInterface> PressureRing;
};

UCLASS(BlueprintType)
class BOLDERSDEMO_API UAttackLoadout : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack") TArray<FAttackModule> Modules;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation") TObjectPtr<UElementalReactionAppearance> ReactionAppearance;
};
