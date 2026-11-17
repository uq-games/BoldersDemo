#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackProjectile.generated.h"

class ABaseCharacter;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// Spawned by the server at the montage release notify. Impact, not release, applies damage.
UCLASS()
class BOLDERSDEMO_API AAttackProjectile : public AActor
{
	GENERATED_BODY()
public:
	AAttackProjectile();
	void InitializeShot(ABaseCharacter* Source, float Damage, const FVector& Direction);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UProjectileMovementComponent> Movement;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
private:
	UPROPERTY(Transient) TWeakObjectPtr<ABaseCharacter> Shooter;
	float ShotDamage = 0.f;
	bool bImpacted = false;
};
