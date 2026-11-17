#include "Characters/FactionRingComponent.h"
#include "Characters/BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

UFactionRingComponent::UFactionRingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetAbsolute(true, true, true);
	SetVisibility(false);
	FadeScreenSize = 0.f;
}

void UFactionRingComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		SetVisibility(false);
		return;
	}
	if (const auto* OwnerCharacter = Cast<ABaseCharacter>(GetOwner()))
		AddTickPrerequisiteComponent(OwnerCharacter->GetCharacterMovement());
	UpdateRing();
}

void UFactionRingComponent::SetSelected(bool bSelected)
{
	if (bLocallySelected == bSelected) return;
	bLocallySelected = bSelected;
	if (GetNetMode() != NM_DedicatedServer) UpdateAppearance();
}

void UFactionRingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateRing(DeltaTime);
}

void UFactionRingComponent::UpdateAppearance()
{
	const auto* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerCharacter) return;
	if (!RingMaterialInstance && GetDecalMaterial())
		RingMaterialInstance = CreateDynamicMaterialInstance();
	if (!RingMaterialInstance) return;
	const FLinearColor Color = OwnerCharacter->GetFaction() == ECharacterFaction::Enemy ? EnemyColor : PlayerColor;
	const float Brightness = bLocallySelected ? SelectedBrightness : NormalBrightness;
	if (Color != AppliedColor)
	{
		RingMaterialInstance->SetVectorParameterValue(TEXT("RingColor"), Color);
		AppliedColor = Color;
	}
	if (Brightness != AppliedBrightness)
	{
		RingMaterialInstance->SetScalarParameterValue(TEXT("RingBrightness"), Brightness);
		AppliedBrightness = Brightness;
	}
}

void UFactionRingComponent::UpdateRing(float DeltaTime)
{
	const auto* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerCharacter || GetNetMode() == NM_DedicatedServer || !OwnerCharacter->IsAlive()
		|| OwnerCharacter->GetFaction() == ECharacterFaction::Neutral || OwnerCharacter->IsHidden())
	{
		SetVisibility(false);
		CachedGround.Reset();
		return;
	}
	// Reads replicated faction/health on each client, without extra RPCs or delegates.
	UpdateAppearance();
	if (!RingMaterialInstance) { SetVisibility(false); return; }
	const FVector Feet = OwnerCharacter->GetActorLocation()
		- FVector::UpVector * OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TimeSinceGroundTrace += FMath::Max(DeltaTime, 0.f);
	const UPrimitiveComponent* GroundComponent = CachedGround.Get();
	const UObject* MovementBaseObject = OwnerCharacter->GetMovementBaseObject();
	const UPrimitiveComponent* MovementBase = Cast<UPrimitiveComponent>(MovementBaseObject);
	const bool bFixedGround = GroundComponent && GroundComponent->Mobility == EComponentMobility::Static
		&& !GroundComponent->IsSimulatingPhysics()
		&& GroundComponent->GetComponentTransform().Equals(CachedGroundTransform)
		&& (!MovementBaseObject || (MovementBase && MovementBase->Mobility == EComponentMobility::Static && !MovementBase->IsSimulatingPhysics()));
	const bool bStationary = Feet.Equals(LastTracedFeet, 0.01f)
		&& OwnerCharacter->GetVelocity().IsNearlyZero()
		&& !OwnerCharacter->GetCharacterMovement()->IsFalling();
	// Keep Tick and appearance updates responsive. Reuse only an unchanged static surface.
	if (IsVisible() && bFixedGround && bStationary && TimeSinceGroundTrace < StationaryTraceInterval) return;
	TimeSinceGroundTrace = 0.f;
	LastTracedFeet = Feet;
	CachedGround.Reset();
#if WITH_DEV_AUTOMATION_TESTS
	++GroundTraceCount;
#endif
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FactionRingGround), true, OwnerCharacter);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Ground;
	const bool bGrounded = GetWorld()->LineTraceSingleByObjectType(Ground,
		Feet + FVector::UpVector * TraceAboveFeet, Feet - FVector::UpVector * TraceBelowFeet, Objects, Params);
	if (!bGrounded || Ground.bStartPenetrating || Ground.ImpactNormal.Z < 0.2f)
	{
		SetVisibility(false);
		return;
	}
	CachedGround = Ground.GetComponent();
	if (CachedGround.IsValid()) CachedGroundTransform = CachedGround->GetComponentTransform();
	DecalSize = FVector(ProjectionDepth, Radius, Radius);
	// Decal local X is its projection direction. Match terrain, not actor yaw/scale.
	SetWorldLocationAndRotation(Ground.ImpactPoint + Ground.ImpactNormal * SurfaceOffset,
		FRotationMatrix::MakeFromX(-Ground.ImpactNormal).Rotator());
	SetVisibility(true);
}
