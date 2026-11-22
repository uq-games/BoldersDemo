#include "Combat/Environment/CombatImpulseProp.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ACombatImpulseProp::ACombatImpulseProp()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(24.f);
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->InitBoxExtent({29.f, 29.f, 42.f});
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->BodyInstance.bSimulatePhysics = true;
	Body->SetLinearDamping(.65f);
	Body->SetAngularDamping(.8f);
	Body->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

void ACombatImpulseProp::BeginPlay()
{
	Super::BeginPlay();
	Body->SetMassOverrideInKg(NAME_None, 24.f);
}
