#include "Combat/Environment/VolcanoHazard.h"

#include "Combat/Environment/CoolingBasinShape.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double Tau = 2.0 * UE_DOUBLE_PI;

	enum EFixtureMesh : uint8
	{
		Vent, Trough, Keg, SpentKeg, Pylon, Crystal
	};

	enum EFixtureEffect : uint8
	{
		VentPressure, VentEruption, VentCooling, TroughSteam, TroughFreeze,
		TroughConduct, KegFuse, KegBlast, PylonCharge, PylonDischarge, ArcLink, NoEffect
	};

	template <typename AssetType>
	AssetType* LoadFixtureAsset(const TCHAR* Folder, const TCHAR* Prefix, const TCHAR* Name)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/%s/%s%s.%s%s"), Folder, Prefix, Name, Prefix, Name);
		ConstructorHelpers::FObjectFinderOptional<AssetType> Asset(*Path, LOAD_Quiet | LOAD_NoWarn);
		return Asset.Get();
	}

	void CreateGroundSection(UProceduralMeshComponent& Component, const TArray<FVector>& Vertices,
		const TArray<int32>& Indices, double Radius)
	{
		TArray<FVector2D> UVs;
		for (const FVector& Vertex : Vertices)
			UVs.Emplace(.5 + Vertex.X / (2.0 * Radius), .5 + Vertex.Y / (2.0 * Radius));

		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Vertices.Num());
		TArray<FColor> Colors;
		Colors.Init(FColor::White, Vertices.Num());
		TArray<FProcMeshTangent> Tangents;
		Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), Vertices.Num());
		Component.CreateMeshSection(0, Vertices, Indices, Normals, UVs, Colors, Tangents, false);
	}

	void AppendBasinBand(TArray<FVector>& Vertices, TArray<int32>& Indices, double Radius,
		double OuterOffset, double OuterHeight, double InnerOffset, double InnerHeight)
	{
		using namespace CoolingBasinShape;
		const double HalfWidth = Radius * HalfWidthRatio;
		const auto Outer = Outline(Radius + OuterOffset, HalfWidth + OuterOffset, Radius * CornerRatio);
		const auto Inner = Outline(Radius + InnerOffset, HalfWidth + InnerOffset, Radius * CornerRatio);
		for (int32 Index = 0; Index < CornerCount; ++Index)
		{
			const int32 Next = (Index + 1) % CornerCount;
			const int32 Base = Vertices.Num();
			Vertices.Append({FVector(Outer[Index], OuterHeight), FVector(Inner[Index], InnerHeight),
				FVector(Outer[Next], OuterHeight), FVector(Inner[Next], InnerHeight)});
			Indices.Append({Base, Base + 1, Base + 2, Base + 2, Base + 1, Base + 3});
		}
	}

}

void AVolcanoHazard::LoadPresentationAssets()
{
	for (const TCHAR* Name : {TEXT("PressureVent"), TEXT("CoolingTrough"), TEXT("PowderKeg"),
		TEXT("PowderKegSpent"), TEXT("ArcPylon"), TEXT("ArcCrystal")})
	{
		FixtureMeshes.Add(LoadFixtureAsset<UStaticMesh>(TEXT("Meshes"), TEXT("SM_Foundry"), Name));
	}
	for (const TCHAR* Name : {TEXT("VentPressure"), TEXT("VentEruption"), TEXT("VentCooling"),
		TEXT("TroughSteam"), TEXT("TroughFreeze"), TEXT("TroughConduct"), TEXT("KegFuse"), TEXT("KegBlast"),
		TEXT("PylonCharge"), TEXT("PylonDischarge"), TEXT("ArcLink")})
	{
		FixtureEffects.Add(LoadFixtureAsset<UNiagaraSystem>(TEXT("Effects"), TEXT("NS_Foundry"), Name));
	}
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Water(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Materials/M_FoundryCoolingBasin.M_FoundryCoolingBasin"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fire(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/M_Concept_FireballHotCore.M_Concept_FireballHotCore"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Orb(
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/M_ArcaneOrb.M_ArcaneOrb"));
	static ConstructorHelpers::FObjectFinder<UElementalReactionAppearance> ReactionAppearance(
		TEXT("/Game/BoldersDemo/Assets/Combat/DA_ElementalReactionAppearance.DA_ElementalReactionAppearance"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> EruptCue(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio/SW_VolcanoEruption.SW_VolcanoEruption"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> BlastCue(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio/SW_VolcanoBarrelBlast.SW_VolcanoBarrelBlast"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> BoltCue(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio/SW_VolcanoOrbDischarge.SW_VolcanoOrbDischarge"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> SteamCue(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio/SW_VolcanoWaterSteam.SW_VolcanoWaterSteam"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> FreezeCue(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio/SW_VolcanoWaterFreeze.SW_VolcanoWaterFreeze"),
		LOAD_Quiet | LOAD_NoWarn);

	PoolMaterial = Water.Get();
	FireCoreMaterial = Fire.Object;
	OrbMaterial = Orb.Object;
	IceMaterial = LoadFixtureAsset<UMaterialInterface>(TEXT("Materials"), TEXT("M_FoundryFixture_"), TEXT("Ice"));
	Reactions = ReactionAppearance.Object;
	EruptionSound = EruptCue.Get();
	BarrelSound = BlastCue.Get();
	OrbSound = BoltCue.Get();
	SteamSound = SteamCue.Get();
	FreezeSound = FreezeCue.Get();
}

void AVolcanoHazard::ConfigurePresentation()
{
	// Existing placed actors can carry material overrides and transforms from their old preview primitives.
	for (UStaticMeshComponent* Part : {Body.Get(), Core.Get()})
	{
		Part->EmptyOverrideMaterials();
		Part->SetRelativeTransform(FTransform::Identity);
	}

	WaterSurface->SetVisibility(Kind == EVolcanoHazardKind::Puddle);
	Core->SetStaticMesh(Kind == EVolcanoHazardKind::LightningOrb ? GetFixtureMesh(Crystal) : nullptr);
	Core->SetVisibility(Core->GetStaticMesh() != nullptr);
	Body->SetVisibility(true);
	BuildGroundMark();
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		AttackTarget->SetRelativeLocation(FVector(0, 0, 33));
		AttackTarget->SetSphereRadius(51.f);
		Body->SetStaticMesh(GetFixtureMesh(Vent));
		Glow->SetLightColor(FLinearColor(1.f, .21f, .05f));
		break;
	case EVolcanoHazardKind::Puddle:
		AttackTarget->SetRelativeLocation(FVector(0, 0, 13));
		AttackTarget->SetSphereRadius(FMath::Clamp(GetEffectRadius() * .30f, 36.f, 55.f));
		Body->SetStaticMesh(GetFixtureMesh(Trough));
		Body->SetRelativeScale3D(FVector(GetEffectRadius() / 160.f, GetEffectRadius() / 160.f, 1.0));
		BuildCoolingBasin();
		Glow->SetLightColor(FLinearColor(.26f, .61f, 1.f));
		break;
	case EVolcanoHazardKind::ExplosiveBarrel:
		AttackTarget->SetRelativeLocation(FVector(0, 0, 66));
		AttackTarget->SetSphereRadius(66.f);
		Body->SetStaticMesh(GetFixtureMesh(Keg));
		Glow->SetLightColor(FLinearColor(1.f, .3f, .08f));
		break;
	case EVolcanoHazardKind::LightningOrb:
		AttackTarget->SetRelativeLocation(FVector(0, 0, 108));
		AttackTarget->SetSphereRadius(49.f);
		Body->SetStaticMesh(GetFixtureMesh(Pylon));
		Core->SetRelativeLocation(AttackTarget->GetRelativeLocation());
		Glow->SetLightColor(FLinearColor(.28f, .52f, 1.f));
		break;
	}
}

void AVolcanoHazard::BuildCoolingBasin()
{
	using namespace CoolingBasinShape;
	const double Size = GetEffectRadius();
	const auto Profile = Outline(Size, Size * HalfWidthRatio, Size * CornerRatio);
	TArray<FVector> Vertices{FVector::ZeroVector};
	TArray<int32> Indices;
	for (int32 Index = 0; Index < CornerCount; ++Index)
	{
		Vertices.Emplace(Profile[Index], 0.0);
		Indices.Append({0, (Index + 1) % CornerCount + 1, Index + 1});
	}
	CreateGroundSection(*WaterSurface, Vertices, Indices, Size);

}

void AVolcanoHazard::BuildGroundMark()
{
	const double Size = GetEffectRadius();
	if (Kind == EVolcanoHazardKind::Puddle)
	{
		TArray<FVector> Vertices;
		TArray<int32> Indices;
		AppendBasinBand(Vertices, Indices, Size, 14, 20, 6, 20);
		CreateGroundSection(*GroundMark, Vertices, Indices, Size);
		return;
	}
	constexpr int32 DashCount = 14;
	constexpr int32 StepsPerDash = 4;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	for (int32 Dash = 0; Dash < DashCount; ++Dash)
	{
		for (int32 Step = 0; Step <= StepsPerDash; ++Step)
		{
			const double Angle = (Dash + .86 * Step / StepsPerDash) * Tau / DashCount;
			const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
			const int32 Index = Vertices.Num();
			Vertices.Add(Radial * (Size - 6.0));
			Vertices.Add(Radial * Size);
			if (Step > 0) Indices.Append({Index - 2, Index, Index - 1, Index - 1, Index, Index + 1});
		}
	}
	CreateGroundSection(*GroundMark, Vertices, Indices, Size);
}

void AVolcanoHazard::SetLocalFocus(bool bFocused)
{
	for (UStaticMeshComponent* Part : {Body.Get(), Core.Get()})
	{
		Part->SetCustomDepthStencilValue(200);
		Part->SetRenderCustomDepth(bFocused);
	}
}

void AVolcanoHazard::UpdatePresentation()
{
	using State = EVolcanoHazardState;
	const bool bWarning = Phase.State == State::Warning;
	const bool bActive = Phase.State == State::Active;
	const bool bCharged = Kind == EVolcanoHazardKind::Puddle || Kind == EVolcanoHazardKind::LightningOrb;
	const bool bAttackable = CanReceiveAttack();
	if (Phase.bEncounterEnded) SetLocalFocus(false);
	AttackTarget->SetCollisionEnabled(bAttackable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	OrbPedestalTarget->SetCollisionEnabled(Kind == EVolcanoHazardKind::LightningOrb && bAttackable
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (GetNetMode() == NM_DedicatedServer) return;

	GroundMark->SetVisibility(bWarning || bActive);
	GroundMark->SetMaterial(0, bCharged ? OrbMaterial.Get() : FireCoreMaterial.Get());
	float LightIntensity = 0.f;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		LightIntensity = bActive ? 2600.f : (bWarning ? 700.f : 65.f);
		break;
	case EVolcanoHazardKind::Puddle:
		WaterSurface->SetMaterial(0, Phase.State == State::Frozen ? IceMaterial.Get() : PoolMaterial.Get());
		LightIntensity = bActive ? 820.f : (Phase.State == State::Frozen ? 245.f : 0.f);
		break;
	case EVolcanoHazardKind::ExplosiveBarrel:
	{
		const bool bIntact = Phase.State == State::Ready || bWarning;
		Body->SetStaticMesh(GetFixtureMesh(bIntact ? Keg : SpentKeg));
		LightIntensity = bActive ? 3900.f : (bWarning ? 900.f : (bIntact ? 55.f : 0.f));
		break;
	}
	case EVolcanoHazardKind::LightningOrb:
		Core->SetVisibility(Phase.State != State::Cooling);
		LightIntensity = bActive ? 4300.f : (bWarning ? 1800.f : (Phase.State == State::Ready ? 230.f : 0.f));
		break;
	}
	Glow->SetRelativeLocation(AttackTarget->GetRelativeLocation() + FVector(0, 0, 35));
	Glow->SetIntensity(LightIntensity);
	if (bPhasePresentationReady) UpdatePhaseEffect();
}

void AVolcanoHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Time = GetWorld()->GetTimeSeconds();
	if (Kind == EVolcanoHazardKind::LightningOrb && Phase.State != EVolcanoHazardState::Cooling)
	{
		const double Bob = FMath::Sin(Time * 2.3) * 6;
		Core->SetRelativeLocation(FVector(0, 0, 108 + Bob));
		Core->SetRelativeRotation(FRotator(0, Time * 24, Time * 9));
	}
}

void AVolcanoHazard::UpdatePhaseEffect()
{
	using State = EVolcanoHazardState;
	if (Phase.bEncounterEnded)
	{
		PhaseEffect->DeactivateImmediate();
		return;
	}

	EFixtureEffect Effect = NoEffect;
	double Height = 0.0;
	bool bBurst = false;
	switch (Kind)
	{
	case EVolcanoHazardKind::Eruption:
		Height = 28;
		if (Phase.State == State::Warning) Effect = VentPressure;
		if (Phase.State == State::Active) Effect = VentEruption;
		if (Phase.State == State::Cooling) Effect = VentCooling;
		break;
	case EVolcanoHazardKind::Puddle:
		Height = 17;
		if (Phase.State == State::Steaming) Effect = TroughSteam;
		if (Phase.State == State::Active) Effect = TroughConduct;
		if (Phase.State == State::Frozen)
		{
			Effect = TroughFreeze;
			bBurst = true;
		}
		break;
	case EVolcanoHazardKind::ExplosiveBarrel:
		Height = Phase.State == State::Warning ? 126.0 : 59.0;
		if (Phase.State == State::Warning) Effect = KegFuse;
		if (Phase.State == State::Active)
		{
			Effect = KegBlast;
			bBurst = true;
		}
		break;
	case EVolcanoHazardKind::LightningOrb:
		Height = 108;
		if (Phase.State == State::Warning) Effect = PylonCharge;
		if (Phase.State == State::Active)
		{
			Effect = PylonDischarge;
			bBurst = true;
		}
		break;
	}

	UNiagaraSystem* System = Effect != NoEffect ? GetFixtureEffect(Effect) : nullptr;
	if (PresentedRevision == Phase.Revision && PresentedEffect == System) return;
	PresentedRevision = Phase.Revision;
	PresentedEffect = System;
	if (!System || (bBurst && GetPhaseAge() >= .55f))
	{
		PhaseEffect->Deactivate();
		return;
	}

	PhaseEffect->SetRelativeLocation(FVector(0, 0, Height));
	if (PhaseEffect->GetAsset() != System) PhaseEffect->SetAsset(System);
	PhaseEffect->Activate(true);
}

bool AVolcanoHazard::PlayConductLink(const FVector& Start, const FVector& End) const
{
	auto* System = GetFixtureEffect(ArcLink);
	const FVector Delta = End - Start;
	if (!System || Delta.SizeSquared() < 100.f) return false;
	const FVector Center = (Start + End) * .5;
	const FRotator Rotation = FRotationMatrix::MakeFromXZ(Delta, FVector::UpVector).Rotator();
	return UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Center, Rotation,
		FVector(Delta.Size() / 100.0, .86, 1), true, true, ENCPoolMethod::AutoRelease) != nullptr;
}

void AVolcanoHazard::PlayPhaseSound(float PhaseAge) const
{
	USoundBase* Sound = nullptr;
	float Volume = 1.f;
	switch (Phase.State)
	{
	case EVolcanoHazardState::Active:
		switch (Kind)
		{
		case EVolcanoHazardKind::Eruption: Sound = EruptionSound; Volume = .8f; break;
		case EVolcanoHazardKind::ExplosiveBarrel: Sound = BarrelSound; break;
		case EVolcanoHazardKind::LightningOrb: Sound = OrbSound; Volume = .9f; break;
		case EVolcanoHazardKind::Puddle: Sound = OrbSound; Volume = .5f; break;
		}
		break;
	case EVolcanoHazardState::Steaming: Sound = SteamSound; break;
	case EVolcanoHazardState::Frozen: Sound = FreezeSound; break;
	default: break;
	}
	if (Sound)
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume, 1.f,
			FMath::Min(PhaseAge, .08f), SoundAttenuation);
}

UStaticMesh* AVolcanoHazard::GetFixtureMesh(int32 Index) const
{
	return FixtureMeshes.IsValidIndex(Index) ? FixtureMeshes[Index].Get() : nullptr;
}

UNiagaraSystem* AVolcanoHazard::GetFixtureEffect(int32 Index) const
{
	return FixtureEffects.IsValidIndex(Index) ? FixtureEffects[Index].Get() : nullptr;
}
