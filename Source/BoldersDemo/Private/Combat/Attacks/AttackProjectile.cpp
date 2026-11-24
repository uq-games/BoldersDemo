#include "Combat/Attacks/AttackProjectile.h"
#include "Characters/BaseCharacter.h"
#include "Combat/Presentation/AttackPresentation.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/Environment/CombatCollision.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AAttackProjectile::AAttackProjectile()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	InitialLifeSpan = 5.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(4.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(BoldersCollision::Interactable, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnImpact);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnPawnOverlap);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	FlightGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlightGlow"));
	FlightGlow->SetupAttachment(Collision);
	FlightGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlightGlow->SetCanEverAffectNavigation(false);
	FlightGlow->SetCastShadow(false);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 1200.f;
	Movement->MaxSpeed = 1200.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bRotationFollowsVelocity = true;
}

void AAttackProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AAttackProjectile, Loadout, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AAttackProjectile, ModuleId, COND_InitialOnly);
}

void AAttackProjectile::InitializeShot(ABaseCharacter* Source, float Damage, const FVector& Direction)
{
	check(HasAuthority());
	Shooter = Source;
	ShotDamage = Damage;
	Collision->IgnoreActorWhenMoving(Source, true);
	Movement->Velocity = Direction.GetSafeNormal() * Movement->InitialSpeed;
}

void AAttackProjectile::InitializeModuleShot(ABaseCharacter* Source, const FAttackModule& Module,
	const FVector& Direction, float Damage, const TArray<TWeakObjectPtr<ABaseCharacter>>& PreviousHits)
{
	Loadout = Source->Combat->AttackLoadout;
	ModuleId = Module.Id;
	Movement->InitialSpeed = FMath::Max(Module.Speed, 1.f);
	Movement->MaxSpeed = Movement->InitialSpeed;
	const float TravelLimit = PreviousHits.IsEmpty() ? Module.Range + 155.f : Module.Radius + 20.f;
	InitialLifeSpan = FMath::Clamp(TravelLimit / Movement->InitialSpeed, .1f, 5.f);
	InitializeShot(Source, Damage, Direction);
	HitCharacters = PreviousHits;
	for (const auto& Previous : HitCharacters)
		if (Previous.IsValid()) Collision->IgnoreActorWhenMoving(Previous.Get(), true);
	if (Module.Pattern == EAttackPattern::Wave)
	{
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Collision->SetSphereRadius(14.f);
		InitialLifeSpan = (Module.Range + 80.f) / Movement->InitialSpeed;
	}
}

const FAttackModule* AAttackProjectile::GetModule() const
{
	return Loadout && !ModuleId.IsNone()
		? Loadout->Modules.FindByPredicate([this](const FAttackModule& Entry){ return Entry.Id == ModuleId; })
		: nullptr;
}

void AAttackProjectile::RefreshFlightVisual()
{
	if (!HasActorBegunPlay() || bFlightVisualConfigured || GetNetMode() == NM_DedicatedServer) return;
	if (const FAttackModule* Module = GetModule())
	{
		AttackPresentation::ConfigureProjectile(*this, *Module);
		bFlightVisualConfigured = true;
	}
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
	else if (const FAttackModule* Module = GetModule(); Module && Module->Pattern == EAttackPattern::Wave)
	{
		LastSweepPosition = GetActorLocation();
		AddTickPrerequisiteComponent(Movement);
		SetActorTickEnabled(true);
	}
	RefreshFlightVisual();
}

void AAttackProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) CheckWavePath(GetActorLocation());
}

void AAttackProjectile::CheckWavePath(const FVector& End)
{
	ABaseCharacter* Source = Shooter.Get();
	const FAttackModule* Module = GetModule();
	if (!Source || !Module) return;
	const FVector Midpoint = (LastSweepPosition + End) * .5f;
	TArray<ABaseCharacter*> Victims;
	FElementalHitBatch Batch;
	for (ABaseCharacter* Target : AttackResolution::FindTargets(*Source, Midpoint, Module->Radius + FVector::Dist(Midpoint, End)))
	{
		if (HitCharacters.Contains(Target)) continue;
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(TargetLocation, LastSweepPosition, End);
		if (FVector::DistSquared(Closest, TargetLocation) > FMath::Square(Module->Radius)
			|| !AttackResolution::HasClearPath(*Source, *Target, Closest, this)) continue;
		HitCharacters.Add(Target);
		Victims.Add(Target);
		Batch.DirectTargets.Add(Target);
	}
	for (ABaseCharacter* Target : Victims)
	{
		const FVector Contact = Target->GetActorLocation();
		if (!AttackResolution::ApplyHit(*Source, *Target, *Module, ShotDamage, &Batch, this)) continue;
		Target->ApplyCombatImpulse(GetActorForwardVector().GetSafeNormal2D() * Module->ImpactPush
			+ FVector(0, 0, Module->ImpactPush * .2f));
		Source->MulticastModuleVisual(ModuleId, EAttackVisualStage::Impact, Contact, Contact + GetActorForwardVector() * 60.f);
	}
	LastSweepPosition = End;
}

void AAttackProjectile::Bounce(const FVector& Location, const FAttackModule& Module)
{
	if (HitCharacters.Num() >= 3 || !Shooter.IsValid()) return;
	auto& Source = *Shooter.Get();
	auto Candidates = AttackResolution::FindTargets(Source, Location, Module.Radius);
	Candidates.Sort([&Location](const ABaseCharacter& A, const ABaseCharacter& B)
	{
		return FVector::DistSquared(Location, A.GetActorLocation()) < FVector::DistSquared(Location, B.GetActorLocation());
	});
	for (ABaseCharacter* Target : Candidates)
	{
		if (HitCharacters.Contains(Target) || FVector::DistSquared(Location, Target->GetActorLocation()) > FMath::Square(Module.Radius)
			|| !AttackResolution::HasClearPath(Source, *Target, Location, this)) continue;
		const FVector Direction = (Target->GetActorLocation() - Location).GetSafeNormal();
		const FTransform Spawn(Direction.Rotation(), Location + Direction * 8.f);
		auto* Arrow = GetWorld()->SpawnActorDeferred<AAttackProjectile>(GetClass(), Spawn, &Source, &Source,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Arrow) return;
		Arrow->InitializeModuleShot(&Source, Module, Direction, ShotDamage * .72f, HitCharacters);
		Arrow->FinishSpawning(Spawn);
		Source.MulticastModuleVisual(ModuleId, EAttackVisualStage::Ricochet, Location, Target->GetActorLocation());
		return;
	}
}

void AAttackProjectile::OnImpact(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherBody, FVector, const FHitResult& Hit)
{
	ResolveImpact(OtherActor, OtherBody, Hit.ImpactPoint, Hit.ImpactNormal, Hit.Location);
}

void AAttackProjectile::OnPawnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherBody,
	int32, bool bFromSweep, const FHitResult& Hit)
{
	const auto* Source = Shooter.Get();
	const auto* Victim = Cast<ABaseCharacter>(OtherActor);
	if (!HasAuthority() || bImpacted || !Source || !Victim || !Victim->IsAlive() || !Source->IsHostileTo(Victim)
		|| HitCharacters.Contains(Victim)) return;
	const FVector Point = bFromSweep ? FVector(Hit.ImpactPoint) : GetActorLocation();
	const FVector Normal = bFromSweep ? FVector(Hit.ImpactNormal) : -GetActorForwardVector();
	ResolveImpact(OtherActor, OtherBody, Point, Normal, GetActorLocation());
}

void AAttackProjectile::ResolveImpact(AActor* OtherActor, UPrimitiveComponent* OtherBody,
	const FVector& Point, const FVector& Normal, const FVector& SweepEnd)
{
	if (!HasAuthority() || bImpacted || OtherActor == Shooter.Get()) return;
	bImpacted = true;
	ABaseCharacter* Source = Shooter.Get();
	auto* Victim = Cast<ABaseCharacter>(OtherActor);
	const FAttackModule* Module = GetModule();
	if (Source && Module)
	{
		auto* Hazard = Cast<AVolcanoHazard>(OtherActor);
		if (Hazard && Module->Pattern != EAttackPattern::Blast) Hazard->ReceiveAttack(Source, Module->Element);
		if (Module->Pattern == EAttackPattern::Blast)
		{
			const FVector BlastOrigin = Point + Normal * 8.f;
			FElementalHitBatch Batch;
			TArray<TPair<ABaseCharacter*, float>> Victims;
			for (ABaseCharacter* Target : AttackResolution::FindTargets(*Source, BlastOrigin, Module->Radius))
			{
				const float Distance = FVector::Dist(BlastOrigin, Target->GetActorLocation());
				if (Target == Victim || (Distance <= Module->Radius
					&& AttackResolution::HasClearPath(*Source, *Target, BlastOrigin, this)))
				{
					const float DistanceRatio = Distance / FMath::Max(1.f, Module->Radius);
					const float Damage = Target == Victim ? ShotDamage : ShotDamage * FMath::Lerp(.9f, .55f, FMath::Clamp(DistanceRatio, 0.f, 1.f));
					Victims.Emplace(Target, Damage);
					Batch.DirectTargets.Add(Target);
				}
			}
			for (const auto& Entry : Victims)
				AttackResolution::ApplyHit(*Source, *Entry.Key, *Module, Entry.Value, &Batch, this);
			AttackResolution::ApplyPressure(*Source, BlastOrigin, GetActorForwardVector(), Module->Radius, Module->ImpactPush, Batch, this);
			if (Hazard) Hazard->ReceiveAttack(Source, Module->Element);
			AttackResolution::TriggerHazardsInArea(*Source, BlastOrigin, Module->Radius, Module->Element, Hazard);
		}
		else if (Module->Pattern == EAttackPattern::Wave)
		{
			CheckWavePath(SweepEnd);
			if (OtherBody && OtherBody->IsSimulatingPhysics() && Module->ImpactPush > 0.f)
				OtherBody->AddImpulse(GetActorForwardVector() * Module->ImpactPush, NAME_None, true);
		}
		else if (IsValid(Victim) && AttackResolution::ApplyHit(*Source, *Victim, *Module, ShotDamage, nullptr, this)
			&& Module->Pattern == EAttackPattern::Ricochet)
		{
			HitCharacters.Add(Victim);
			Bounce(Point, *Module);
		}
		Source->MulticastModuleVisual(ModuleId, EAttackVisualStage::Impact, Point, Point + GetActorForwardVector() * 70.f);
	}
	else if (Source && IsValid(Victim))
	{
		Victim->ReceiveDamageAt(Source, ShotDamage, Point);
	}
	else if (Source)
	{
		if (auto* Interactive = Cast<AVolcanoHazard>(OtherActor))
			Interactive->ReceiveAttack(Source, EDamageElement::Physical);
	}
	Destroy();
}
