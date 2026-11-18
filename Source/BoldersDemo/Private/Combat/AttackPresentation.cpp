#include "Combat/AttackPresentation.h"

#include "Camera/PlayerCameraManager.h"
#include "Characters/BaseCharacter.h"
#include "Combat/AttackProjectile.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Weapon.h"
#include "AttackResolution.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DecalActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	UStaticMesh* PlaneMesh()
	{
		static TSoftObjectPtr<UStaticMesh> Mesh(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
		return Mesh.LoadSynchronous();
	}

	void SetSurface(UStaticMeshComponent& Surface, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		Surface.SetMobility(EComponentMobility::Movable);
		Surface.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Surface.SetCanEverAffectNavigation(false);
		Surface.SetCastShadow(false);
		Surface.SetStaticMesh(Mesh);
		Surface.SetMaterial(0, Material);
	}

	AStaticMeshActor* SpawnSurface(UWorld& World, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location,
		const FRotator& Rotation, const FVector& Scale, float Lifetime)
	{
		if (!Material || !Mesh) return nullptr;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World.SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
		if (!Actor) return nullptr;
		Actor->SetActorEnableCollision(false);
		auto* Surface = Actor->GetStaticMeshComponent();
		SetSurface(*Surface, Mesh, Material);
		Actor->SetActorScale3D(Scale);
		if (auto* Instance = Surface->CreateDynamicMaterialInstance(0))
		{
			Instance->SetScalarParameterValue(TEXT("StartTime"), World.GetTimeSeconds());
			Instance->SetScalarParameterValue(TEXT("Lifetime"), Lifetime);
		}
		Actor->SetLifeSpan(Lifetime);
		return Actor;
	}

	FVector ViewDirection(const AActor& Context, const FVector& Location)
	{
		if (const auto* Camera = UGameplayStatics::GetPlayerCameraManager(&Context, 0))
			return (Camera->GetCameraLocation() - Location).GetSafeNormal();
		return FVector(-.5, 0, .86);
	}

	FVector FindGroundForVisual(const AActor& Source, const FVector& Near, FVector& Normal)
	{
		FHitResult Hit;
		const FCollisionObjectQueryParams GroundObjects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
		if (!Source.GetWorld()->LineTraceSingleByObjectType(Hit, Near + FVector(0, 0, 12), Near - FVector(0, 0, 230),
			GroundObjects, FCollisionQueryParams(SCENE_QUERY_STAT(AttackVisualGround), false, &Source))) return Near;
		Normal = Hit.ImpactNormal;
		return Hit.ImpactPoint;
	}

	void PlaceGroundMark(const AActor& Source, UMaterialInterface* Material, float Radius, const FVector& Near, float Lifetime)
	{
		if (!Material || Radius <= 0.f) return;
		FVector Normal = FVector::UpVector;
		const FVector Ground = FindGroundForVisual(Source, Near, Normal);
		const float Diameter = Radius * .02f;
		SpawnSurface(*Source.GetWorld(), PlaneMesh(), Material, Ground + Normal * 5.f,
			FRotationMatrix::MakeFromZ(Normal).Rotator(), {Diameter, Diameter, 1.f}, Lifetime);
	}

	void DrawBeam(const AActor& Source, UMaterialInterface* Material, const FVector& From, const FVector& To,
		float Width, float Lifetime)
	{
		const FVector Center = (From + To) * .5;
		const FVector Direction = (To - From).GetSafeNormal();
		if (Direction.IsNearlyZero()) return;
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Direction, ViewDirection(Source, Center)).Rotator();
		SpawnSurface(*Source.GetWorld(), PlaneMesh(), Material, Center, Rotation,
			{FVector::Dist(From, To) * .01, Width * .01, 1.f}, Lifetime);
	}
}

void AttackPresentation::Play(ABaseCharacter& Source, const FAttackModule& Module, EAttackVisualStage Stage,
	const FVector& Location, const FVector& End)
{
	UWorld& World = *Source.GetWorld();
	const FVector Direction = (End - Location).GetSafeNormal();
	const FVector View = ViewDirection(Source, Location);
	const bool bMelee = Module.Pattern == EAttackPattern::SingleMelee || Module.Pattern == EAttackPattern::Cleave;

	switch (Stage)
	{
	case EAttackVisualStage::Cast:
		if (bMelee)
		{
			const float Reach = (Module.Range + 35.f) * .022f;
			const bool bCleave = Module.Pattern == EAttackPattern::Cleave;
			FRotator Facing = Direction.Rotation();
			Facing.Pitch = bCleave ? 9.f : 6.f;
			// Lift the edge of the sweep above the target's cap so it reads from the overhead camera.
			SpawnSurface(World, PlaneMesh(), Module.Cast, Location + FVector(0, 0, bCleave ? 61.f : 47.f), Facing,
				{Reach, Reach, 1.f}, bCleave ? .48f : .3f);
		}
		else
		{
			SpawnSurface(World, PlaneMesh(), Module.Cast, Location + View * 8.f,
				FRotationMatrix::MakeFromXZ(Direction, View).Rotator(), {1.4f, 1.05f, 1.f}, .26f);
		}
		return;
	case EAttackVisualStage::GroundWarning:
		PlaceGroundMark(Source, Module.GroundMark, Module.Radius, Location, Module.LaunchDelay);
		return;
	case EAttackVisualStage::Ricochet:
		DrawBeam(Source, Module.Cast, Location, End, 53.f, .38f);
		return;
	case EAttackVisualStage::SkyImpact:
		DrawBeam(Source, Module.Cast, Location, End, 236.f, .53f);
		break;
	case EAttackVisualStage::Impact:
		break;
	}

	if (Module.ImpactParticles)
	{
		const FRotator Rotation = FRotationMatrix::MakeFromX((Direction + FVector(0, 0, .85)).GetSafeNormal()).Rotator();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(&Source, Module.ImpactParticles,
			Location, Rotation, FVector(Module.Pattern == EAttackPattern::Blast ? 1.2f : .85f));
	}
	const double Width = Module.Pattern == EAttackPattern::Blast ? FMath::Max(1.6, Module.Radius * .017) :
		(Stage == EAttackVisualStage::SkyImpact ? 3.1 : (Module.Pattern == EAttackPattern::Wave ? 2.5 : 1.52));
	SpawnSurface(World, PlaneMesh(), Module.Impact, Location + View * 16.f,
		FRotationMatrix::MakeFromZ(View).Rotator(), {Width, Width, 1.f}, Module.Pattern == EAttackPattern::Blast ? .64f : .45f);
	if (Module.Pattern == EAttackPattern::Blast || Module.Pattern == EAttackPattern::Freeze)
		PlaceGroundMark(Source, Module.GroundMark, Module.Radius, Location, .55f);
	if (Module.PressureRing)
		PlaceGroundMark(Source, Module.PressureRing, Module.Radius, Location, .52f);
}

AActor* AttackPresentation::PlayWindup(ABaseCharacter& Source, const FAttackModule& Module, float Duration)
{
	if (!Module.Windup) return nullptr;
	const FVector Ground = AttackResolution::GroundPoint(Source);
	const FVector Forward = Source.GetActorForwardVector();
	const bool bCaster = Module.Pattern == EAttackPattern::Wave || Module.Pattern == EAttackPattern::IceLance
		|| Module.Pattern == EAttackPattern::SkyStrike || (Module.Pattern == EAttackPattern::Blast && Module.ProjectileMesh);
	FVector Normal = FVector::UpVector;
	const FVector Center = FindGroundForVisual(Source, Ground + Forward * (bCaster ? 67.f : 53.f), Normal);
	auto* Mark = Source.GetWorld()->SpawnActor<ADecalActor>();
	if (!Mark) return nullptr;
	auto* Glyph = Mark->GetDecal();
	// Unreal's decal U and V correspond to local Z and Y respectively.
	Glyph->DecalSize = bCaster ? FVector(18, 142, 160) : FVector(16, 96, 117);
	Glyph->FadeScreenSize = 0.f;
	Glyph->SortOrder = 1;
	Glyph->SetWorldLocationAndRotation(Center + Normal * 2.f, FRotationMatrix::MakeFromXZ(-Normal, Forward).Rotator());
	Mark->SetDecalMaterial(Module.Windup);
	Mark->SetActorEnableCollision(false);
	Mark->SetLifeSpan(Duration);
	// Keep this presentation actor independent of the character's collision and swept movement.
	// A caster remains at the channel origin until release or cancels the windup by moving.
	const AWeapon* Weapon = Source.Combat->GetWeapon();
	if (Weapon && Module.ImpactParticles)
	{
		const FVector Hand = Weapon->bUseWeaponReleasePoint
			? Weapon->Mesh->GetComponentTransform().TransformPosition(Weapon->WeaponReleasePoint)
			: Weapon->Mesh->GetComponentLocation();
		const FVector Focus = Hand + Forward * 26.f + FVector(0, 0, 37.f);
		auto* Sparks = NewObject<UNiagaraComponent>(Mark);
		Sparks->SetAsset(Module.ImpactParticles);
		Sparks->SetupAttachment(Mark->GetRootComponent());
		Sparks->SetAbsolute(true, true, true);
		Sparks->SetWorldTransform(FTransform(FRotator(65.f, Forward.Rotation().Yaw, 0.f),
			Focus, FVector(bCaster ? .54 : .31)));
		Sparks->RegisterComponent();
	}
	return Mark;
}

void AttackPresentation::PlayReaction(ABaseCharacter& Source, const UElementalReactionAppearance& Appearance,
	EElementalReaction Reaction, const FVector& Location, const FVector& End)
{
	const auto* Style = Appearance.Reactions.FindByPredicate([Reaction](const FReactionAppearance& Entry)
	{
		return Entry.Reaction == Reaction;
	});
	if (!Style) return;
	UWorld& World = *Source.GetWorld();
	const FVector View = ViewDirection(Source, Location);
	const bool bOverload = Reaction == EElementalReaction::Overload;
	if (!Location.Equals(End) && Style->Beam) DrawBeam(Source, Style->Beam, End, Location, 74.f, .54f);
	if (Style->Particles)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(&Source, Style->Particles, Location, FRotator(75, 0, 0), FVector(bOverload ? 1.35f : 1.03f));
	const double Scale = bOverload ? 3.84 : 2.22;
	SpawnSurface(World, PlaneMesh(), Style->Burst, Location + View * 23.f,
		FRotationMatrix::MakeFromZ(View).Rotator(), {Scale, Scale, 1.f}, .65f);
	PlaceGroundMark(Source, Style->Ring, bOverload ? 190.f : 105.f, Location, .64f);
	if (Reaction == EElementalReaction::Freeze)
	{
		FVector Normal = FVector::UpVector;
		const FVector Ground = FindGroundForVisual(Source, Location, Normal);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Yaw = Index * 72.f;
			const FVector Offset = FRotator(0, Yaw, 0).Vector() * (Index % 2 == 0 ? 40.f : 27.f);
			SpawnSurface(World, Appearance.CrystalMesh, Appearance.CrystalMaterial, Ground + Offset + FVector(0, 0, 81.f + Index * 8.f),
				FRotator(67.f + Index * 4.f, Yaw, 0), {.58f + .08f * Index, .45f, .45f}, .72f);
		}
	}
}

void AttackPresentation::ConfigureProjectile(AAttackProjectile& Projectile, const FAttackModule& Module)
{
	auto* Mesh = Projectile.Mesh.Get();
	Projectile.FlightGlow->SetVisibility(false);
	const bool bArrow = Module.Element == EDamageElement::Physical || Module.Pattern == EAttackPattern::Freeze
		|| (Module.Pattern == EAttackPattern::Blast && !Module.ProjectileMesh);
	if (Module.ProjectileMesh && !bArrow)
	{
		Mesh->SetStaticMesh(Module.ProjectileMesh);
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
		Mesh->SetRelativeScale3D(FVector::OneVector);
		Mesh->SetRelativeLocation(FVector::ZeroVector);
		if (Module.Pattern == EAttackPattern::Blast) Mesh->SetRelativeScale3D({.58, .51, .51});
	}
	else if (!bArrow)
	{
		Mesh->SetRelativeScale3D({.58, .51, .51});
	}
	if (Module.ProjectileMaterial && !bArrow) Mesh->SetMaterial(0, Module.ProjectileMaterial);
	if (Module.Pattern == EAttackPattern::Blast || Module.Pattern == EAttackPattern::Wave) Mesh->SetCastShadow(false);
	if (!Module.Trail) return;
	const bool bWave = Module.Pattern == EAttackPattern::Wave;
	const float Length = bWave ? 2.5f : (bArrow ? 1.48f : 2.55f);
	const float Width = bWave ? Module.Radius * .027f : (bArrow ? .44f : 1.13f);
	const FVector Center(bWave ? -56.f : -Length * 45.f, 0, bWave ? -26.f : 0.f);
	for (int32 Index = 0; Index < (bWave ? 1 : 2); ++Index)
	{
		auto* Trail = NewObject<UStaticMeshComponent>(&Projectile);
		SetSurface(*Trail, PlaneMesh(), Module.Trail);
		Trail->SetupAttachment(Projectile.GetRootComponent());
		Trail->SetRelativeTransform(FTransform(FRotator(0, 0, Index * 90.f), Center, {Length, Width, 1.f}));
		Trail->RegisterComponent();
	}
}
