#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackProjectile.generated.h"

class ABaseCharacter;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UAttackLoadout;
struct FAttackModule;

// Spawned by the server at the montage release notify. Impact, not release, applies damage.
UCLASS()
class BOLDERSDEMO_API AAttackProjectile : public AActor
{
	GENERATED_BODY()
public:
	AAttackProjectile();
	void InitializeShot(ABaseCharacter* Source, float Damage, const FVector& Direction);
	void InitializeModuleShot(ABaseCharacter* Source, const FAttackModule& Module, const FVector& Direction,
		float Damage, const TArray<TWeakObjectPtr<ABaseCharacter>>& PreviousHits = {});
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UStaticMeshComponent> FlightGlow;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
private:
	UFUNCTION() void RefreshFlightVisual();
	const FAttackModule* GetModule() const;
	void CheckWavePath(const FVector& End);
	void Bounce(const FVector& Location, const FAttackModule& Module);
	UPROPERTY(ReplicatedUsing=RefreshFlightVisual) TObjectPtr<UAttackLoadout> Loadout;
	UPROPERTY(ReplicatedUsing=RefreshFlightVisual) FName ModuleId;
	UPROPERTY(Transient) TWeakObjectPtr<ABaseCharacter> Shooter;
	TArray<TWeakObjectPtr<ABaseCharacter>> HitCharacters;
	FVector LastSweepPosition = FVector::ZeroVector;
	float ShotDamage = 0.f;
	bool bImpacted = false;
	bool bFlightVisualConfigured = false;
};
