#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackFlightParticles.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

// A bounded pool of world-space sparks or spray. It drains after its projectile disappears.
UCLASS(NotBlueprintable, Transient)
class AAttackFlightParticles : public AActor
{
	GENERATED_BODY()

public:
	AAttackFlightParticles();
	void Initialize(AActor& Projectile, UStaticMesh& ParticleMesh, bool bInWater);
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FParticle
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 1.f;
		float Lifetime = .45f;
		float Scale = 1.f;
	};

	static constexpr int32 Capacity = 24;
	void Emit(const FTransform& Frame);

	UPROPERTY(VisibleAnywhere, Category="VFX")
	TObjectPtr<UInstancedStaticMeshComponent> Instances;
	TWeakObjectPtr<AActor> Source;
	FParticle Particles[Capacity];
	FTransform LastFrame;
	TArray<FTransform> Transforms;
	float UntilEmission = 0.f;
	int32 Cursor = 0;
	bool bWater = false;
};
