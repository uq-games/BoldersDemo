#include "Characters/BaseCharacter.h"

#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Presentation/AttackPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

void ABaseCharacter::MulticastModuleVisual_Implementation(FName ModuleId, EAttackVisualStage Stage,
	FVector_NetQuantize Location, FVector_NetQuantize End)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (const FAttackModule* Module = Combat->FindModule(ModuleId))
		AttackPresentation::Play(*this, *Module, Stage, Location, End);
}

void ABaseCharacter::MulticastElementalReaction_Implementation(EElementalReaction Reaction,
	FVector_NetQuantize Location, FVector_NetQuantize End)
{
	if (GetNetMode() == NM_DedicatedServer || !Combat->AttackLoadout) return;
	if (const auto* Appearance = Combat->AttackLoadout->ReactionAppearance.Get())
		AttackPresentation::PlayReaction(*this, *Appearance, Reaction, Location, End);
}

void ABaseCharacter::MulticastModuleWindup_Implementation(FName ModuleId, float Duration, FVector_NetQuantize Aim)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (LocalWindupVisual.IsValid()) LocalWindupVisual->Destroy();
	LocalWindupVisual.Reset();
	if (Duration <= 0.f) return;
	if (const FAttackModule* Module = Combat->FindModule(ModuleId))
		LocalWindupVisual = AttackPresentation::PlayWindup(*this, *Module, Duration, Aim);
}

void ABaseCharacter::MulticastPlayHitReaction_Implementation()
{
	if (IsAlive() && HitReactionMontage) PlayAnimMontage(HitReactionMontage);
}

void ABaseCharacter::SetHoverOutline(bool bEnabled, bool bSelected)
{
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* Part : Meshes)
	{
		Part->SetCustomDepthStencilValue(bSelected ? 201 : 200);
		Part->SetRenderCustomDepth(bEnabled || bSelected);
	}
}

void ABaseCharacter::MulticastAttackVisual_Implementation(FVector_NetQuantize Location, FRotator Facing, bool bImpact)
{
	UMaterialInterface* Material = bImpact ? ImpactMaterial : SwingMaterial;
	if (!Material || GetNetMode() == NM_DedicatedServer) return;
	static const TSoftObjectPtr<UStaticMesh> Plane(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
	UStaticMesh* PlaneMesh = Plane.LoadSynchronous();
	if (!PlaneMesh) return;
	FRotator Rotation(12.f, Facing.Yaw, -18.f);
	if (bImpact)
	{
		if (const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector ViewDirection = (Camera->GetCameraLocation() - Location).GetSafeNormal();
			Rotation = FRotationMatrix::MakeFromZ(ViewDirection).Rotator();
			Location += ViewDirection * 14.f;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Flash = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!Flash) return;
	auto* Surface = Flash->GetStaticMeshComponent();
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->SetStaticMesh(PlaneMesh);
	Flash->SetActorScale3D(FVector(bImpact ? 1.45f : 1.9f));
	auto* Instance = Surface->CreateDynamicMaterialInstance(0, Material);
	if (Instance) Instance->SetScalarParameterValue(TEXT("StartTime"), GetWorld()->GetTimeSeconds());
	Flash->SetLifeSpan(bImpact ? 0.36f : 0.34f);
}

void ABaseCharacter::UpdateFrozenVisual(bool bFrozen)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (bFrozen && FrozenVisualMesh && !LocalFrozenVisual)
	{
		LocalFrozenVisual = NewObject<UStaticMeshComponent>(this, TEXT("FrozenCrystalCrown"));
		LocalFrozenVisual->SetupAttachment(GetRootComponent());
		LocalFrozenVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LocalFrozenVisual->SetCanEverAffectNavigation(false);
		LocalFrozenVisual->SetCastShadow(false);
		LocalFrozenVisual->SetStaticMesh(FrozenVisualMesh);
		const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
		LocalFrozenVisual->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		LocalFrozenVisual->SetRelativeScale3D(FVector(Radius / 42.f));
		LocalFrozenVisual->RegisterComponent();
	}
	else if (!bFrozen && LocalFrozenVisual)
	{
		LocalFrozenVisual->DestroyComponent();
		LocalFrozenVisual = nullptr;
	}
}
