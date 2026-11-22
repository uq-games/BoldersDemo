#include "AttackFlightParticles.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AAttackFlightParticles::AAttackFlightParticles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FlightParticles"));
	SetRootComponent(Instances);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false);
}

void AAttackFlightParticles::Initialize(AActor& Projectile, UStaticMesh& ParticleMesh, bool bInWater)
{
	Source = &Projectile;
	bWater = bInWater;
	LastFrame = Projectile.GetActorTransform();
	Instances->SetStaticMesh(&ParticleMesh);
	Transforms.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector), Capacity);
	Instances->AddInstances(Transforms, false, true, false);
	SetLifeSpan(6.f);
}

void AAttackFlightParticles::Emit(const FTransform& Frame)
{
	const FTransform Placement(Frame.GetRotation(), Frame.GetLocation());
	const float Sign = Cursor % 2 == 0 ? 1.f : -1.f;
	auto& P = Particles[Cursor++ % Capacity];
	P.Age = 0.f;
	const FVector LocalOrigin = bWater
		? FVector(FMath::FRandRange(5.f, 42.f), Sign * FMath::FRandRange(12.f, 63.f), FMath::FRandRange(33.f, 62.f))
		: FVector(FMath::FRandRange(-99.f, -54.f), FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-14.f, 14.f));
	const FVector LocalVelocity = bWater
		? FVector(FMath::FRandRange(95.f, 170.f), Sign * FMath::FRandRange(58.f, 119.f), FMath::FRandRange(103.f, 162.f))
		: FVector(FMath::FRandRange(59.f, 174.f), FMath::FRandRange(-64.f, 64.f), FMath::FRandRange(23.f, 109.f));
	P.Position = Placement.TransformPosition(LocalOrigin);
	P.Velocity = Placement.TransformVectorNoScale(LocalVelocity);
	P.Lifetime = bWater ? FMath::FRandRange(.24f, .42f) : FMath::FRandRange(.25f, .43f);
	P.Scale = FMath::FRandRange(.65f, 1.36f);
}

void AAttackFlightParticles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Transforms.IsEmpty()) return;

	if (const AActor* Projectile = Source.Get())
	{
		const FTransform Current = Projectile->GetActorTransform();
		const float Interval = bWater ? .040f : .026f;
		for (int32 Step = 0; UntilEmission <= DeltaSeconds && Step < 6; ++Step)
		{
			FTransform Frame;
			Frame.Blend(LastFrame, Current, FMath::Clamp(UntilEmission / FMath::Max(DeltaSeconds, UE_SMALL_NUMBER), 0.f, 1.f));
			Emit(Frame);
			UntilEmission += Interval;
		}
		UntilEmission = FMath::Max(0.f, UntilEmission - DeltaSeconds);
		LastFrame = Current;
	}

	bool bActive = false;
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		auto& P = Particles[Index];
		P.Age += DeltaSeconds;
		if (P.Age >= P.Lifetime)
		{
			Transforms[Index].SetScale3D(FVector::ZeroVector);
			continue;
		}
		P.Position += P.Velocity * DeltaSeconds;
		P.Velocity.Z -= (bWater ? 735.f : 110.f) * DeltaSeconds;
		const float Size = P.Scale * (1.f - FMath::Square(P.Age / P.Lifetime));
		Transforms[Index] = FTransform(P.Velocity.Rotation(), P.Position, FVector(Size));
		bActive = true;
	}
	if (!bActive && !Source.IsValid())
	{
		Destroy();
		return;
	}
	Instances->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
