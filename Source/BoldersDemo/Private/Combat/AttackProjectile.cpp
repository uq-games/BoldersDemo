#include "Combat/AttackProjectile.h"
#include "Characters/BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AAttackProjectile::AAttackProjectile()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	InitialLifeSpan = 5.f;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(4.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnImpact);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 1200.f;
	Movement->MaxSpeed = 1200.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bRotationFollowsVelocity = true;
}

void AAttackProjectile::InitializeShot(ABaseCharacter* Source, float Damage, const FVector& Direction)
{
	check(HasAuthority());
	Shooter = Source;
	ShotDamage = Damage;
	Collision->IgnoreActorWhenMoving(Source, true);
	Movement->Velocity = Direction.GetSafeNormal() * Movement->InitialSpeed;
}

void AAttackProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		// Clients display replicated flight; only the server simulates collision and damage.
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Movement->Deactivate();
	}
}

void AAttackProjectile::OnImpact(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (!HasAuthority() || bImpacted || OtherActor == Shooter.Get()) return;
	bImpacted = true;
	if (auto* Victim = Cast<ABaseCharacter>(OtherActor); IsValid(Victim) && Shooter.IsValid())
		Victim->ReceiveDamageFrom(Shooter.Get(), ShotDamage);
	Destroy();
}
