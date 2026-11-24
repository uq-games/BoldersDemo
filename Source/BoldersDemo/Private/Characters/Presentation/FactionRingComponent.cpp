#include "Characters/Presentation/FactionRingComponent.h"
#include "Characters/BaseCharacter.h"
#include "Characters/StatueBoss/StatueBossCharacter.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

UFactionRingComponent::UFactionRingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetAbsolute(true, true, true);
	SetVisibility(false);
	FadeScreenSize = 0.f;
	SetSortOrder(CombatGuideStyle::FootprintSort);
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Footprint(CombatGuideStyle::FootprintMaterialPath);
	FootprintMaterial = Footprint.Object;
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
	if (FootprintMaterial && GetDecalMaterial() != FootprintMaterial)
		SetDecalMaterial(FootprintMaterial);
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
	const bool bEnemy = OwnerCharacter->GetFaction() == ECharacterFaction::Enemy;
	const bool bGuest = OwnerCharacter->PlayerSlot == 2;
	const float Style = bEnemy ? 2.f : bGuest ? 1.f : 0.f;
	FLinearColor Color = bEnemy ? EnemyColor : PlayerColor;
	if (!bEnemy && OwnerCharacter->PlayerSlot == 1) Color = FLinearColor(.16f, .84f, 1.f);
	else if (!bEnemy && bGuest) Color = FLinearColor(1.f, .76f, .23f);
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
	if (Style != AppliedStyle)
	{
		RingMaterialInstance->SetScalarParameterValue(TEXT("MarkStyle"), Style);
		AppliedStyle = Style;
	}
}

void UFactionRingComponent::UpdateRing(float DeltaTime)
{
	const auto* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerCharacter || GetNetMode() == NM_DedicatedServer || !OwnerCharacter->IsAlive()
		|| OwnerCharacter->GetFaction() == ECharacterFaction::Neutral || OwnerCharacter->IsHidden()
		|| (OwnerCharacter->IsA<AStatueBossCharacter>() && !CastChecked<AStatueBossCharacter>(OwnerCharacter)->IsAwakened()))
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
	const FVector Heading = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	TimeSinceGroundTrace += FMath::Max(DeltaTime, 0.f);
	const UPrimitiveComponent* GroundComponent = CachedGround.Get();
	const UObject* MovementBaseObject = OwnerCharacter->GetMovementBaseObject();
	const UPrimitiveComponent* MovementBase = Cast<UPrimitiveComponent>(MovementBaseObject);
	const bool bFixedGround = GroundComponent && GroundComponent->Mobility == EComponentMobility::Static
		&& !GroundComponent->IsSimulatingPhysics()
		&& GroundComponent->GetComponentTransform().Equals(CachedGroundTransform)
		&& (!MovementBaseObject || (MovementBase && MovementBase->Mobility == EComponentMobility::Static && !MovementBase->IsSimulatingPhysics()));
	const bool bStationary = Feet.Equals(LastTracedFeet, 0.01f)
		&& Heading.Equals(LastTracedHeading, .012f)
		&& OwnerCharacter->GetVelocity().IsNearlyZero()
		&& !OwnerCharacter->GetCharacterMovement()->IsFalling();
	// Keep Tick and appearance updates responsive. Reuse only an unchanged static surface.
	if (IsVisible() && bFixedGround && bStationary && TimeSinceGroundTrace < StationaryTraceInterval) return;
	TimeSinceGroundTrace = 0.f;
	LastTracedFeet = Feet;
	LastTracedHeading = Heading;
	CachedGround.Reset();
#if WITH_DEV_AUTOMATION_TESTS
	++GroundTraceCount;
#endif
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FactionRingGround), true, OwnerCharacter);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Ground;
	const FVector TraceStart = Feet + FVector::UpVector * TraceAboveFeet;
	const FVector TraceEnd = Feet - FVector::UpVector * TraceBelowFeet;
	const auto TraceSurface = [&]()
	{
		return GetWorld()->LineTraceSingleByObjectType(Ground, TraceStart, TraceEnd, Objects, Params)
			&& !Ground.bStartPenetrating && Ground.ImpactNormal.Z >= .2f;
	};
	bool bGrounded = TraceSurface();
	if (!bGrounded)
	{
		// A shallow projection can still reach visible geometry around a simple collision support.
		Params.bTraceComplex = false;
		bGrounded = TraceSurface();
	}
	if (!bGrounded)
	{
		SetVisibility(false);
		return;
	}
	CachedGround = Ground.GetComponent();
	if (CachedGround.IsValid()) CachedGroundTransform = CachedGround->GetComponentTransform();
	const FVector Size(ProjectionDepth, Radius / CombatGuideStyle::DecalBoundary, Radius / CombatGuideStyle::DecalBoundary);
	if (!DecalSize.Equals(Size))
	{
		DecalSize = Size;
		MarkRenderStateDirty();
	}
	const FVector Right(-Heading.Y, Heading.X, 0.);
	SetWorldLocationAndRotation(Ground.ImpactPoint + Ground.ImpactNormal * SurfaceOffset,
		FRotationMatrix::MakeFromXY(-Ground.ImpactNormal, Right).Rotator());
	SetVisibility(true);
}
