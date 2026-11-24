#include "Combat/Presentation/WarriorBladeTrail.h"

#include "Combat/Presentation/AnimNotifyState_WeaponTrail.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Combat/Equipment/Weapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	constexpr int32 MaxSamples = 48;
	constexpr int32 VerticesPerSample = 4;
}

AWarriorBladeTrail::AWarriorBladeTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Ribbon = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ribbon"));
	SetRootComponent(Ribbon);
	Ribbon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ribbon->SetCanEverAffectNavigation(false);
	Ribbon->SetCastShadow(false);
}

void AWarriorBladeTrail::Initialize(AWeapon& Weapon, USkeletalMeshComponent& Pose, const UAnimNotifyState_WeaponTrail& Style)
{
	Blade = Weapon.Mesh.Get();
	OriginalMesh = Blade->GetStaticMesh();
	Notify = &Style;
	BladeRoot = Style.BladeRoot;
	BladeTip = Style.BladeTip;
	FadeTime = FMath::Clamp(Style.FadeTime, .03f, .35f);
	TipWidth = Style.TipWidth;
	if (auto* Material = Ribbon->CreateDynamicMaterialInstance(0, Style.TrailMaterial))
	{
		const EDamageElement Coating = Weapon.GetDisplayedCoating();
		Material->SetVectorParameterValue(TEXT("TrailTint"), Coating == EDamageElement::Physical
			? Style.Tint : CombatGuideStyle::ElementColor(Coating));
	}
	Samples.Reserve(MaxSamples);
	Vertices.Reserve(MaxSamples * VerticesPerSample);
	UVs.Reserve(MaxSamples * VerticesPerSample);
	Colors.Reserve(MaxSamples * VerticesPerSample);
	Indices.Reserve((MaxSamples - 1) * 12);
	for (int32 Row = 1; Row < MaxSamples; ++Row)
	{
		for (int32 I = 0; I < VerticesPerSample; I += 2)
		{
			const int32 A = (Row - 1) * VerticesPerSample + I;
			const int32 B = A + VerticesPerSample;
			Indices.Append({A, B, A + 1, A + 1, B, B + 1});
		}
	}
	AddTickPrerequisiteComponent(&Pose);
	// Safety cap for an interrupted montage whose end event never reaches us.
	SetLifeSpan(4.f);
}

void AWarriorBladeTrail::Stop(const UAnimNotifyState_WeaponTrail& Style)
{
	if (Notify.Get() == &Style)
		bSampling = false;
}

void AWarriorBladeTrail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Blade.IsValid() || !OriginalMesh.IsValid() || Blade->GetStaticMesh() != OriginalMesh.Get())
		bSampling = false;
	if (bSampling)
		Sample(Now);
	Samples.RemoveAll([Now, this](const FSample& Point) { return Now - Point.Time >= FadeTime; });
	if (!bSampling && Samples.Num() < 2)
	{
		Destroy();
		return;
	}
	Rebuild(Now);
}

void AWarriorBladeTrail::Sample(double Now)
{
	const FTransform CurrentPose = Blade->GetComponentTransform();
	const FVector Root = CurrentPose.TransformPosition(BladeRoot);
	const FVector Tip = CurrentPose.TransformPosition(BladeTip);
	if (!Samples.IsEmpty())
	{
		const FSample Last = Samples.Last();
		const double Travel = FMath::Max(FVector::Dist(Root, Last.Root), FVector::Dist(Tip, Last.Tip));
		if (Travel < .4)
			return;
		// Never draw a bridge across a teleport, an asset swap, or a long stall.
		if (Travel > 260. || Now - Last.Time >= FadeTime)
			Samples.Reset();
		else
		{
			const int32 Steps = FMath::Clamp(FMath::CeilToInt(Travel / 19.), 1, 5);
			for (int32 Step = 1; Step < Steps; ++Step)
			{
				const float Part = static_cast<float>(Step) / Steps;
				FTransform Pose;
				Pose.Blend(LastPose, CurrentPose, Part);
				Samples.Add({Pose.TransformPosition(BladeRoot), Pose.TransformPosition(BladeTip),
					FMath::Lerp(Last.Time, Now, Part)});
			}
		}
	}
	Samples.Add({Root, Tip, Now});
	LastPose = CurrentPose;
	if (Samples.Num() > MaxSamples)
		Samples.RemoveAt(0, Samples.Num() - MaxSamples, EAllowShrinking::No);
}

void AWarriorBladeTrail::Rebuild(double Now)
{
	if (Samples.Num() < 2)
	{
		Ribbon->SetVisibility(false);
		return;
	}

	Vertices.Reset();
	UVs.Reset();
	Colors.Reset();
	const FVector Origin = GetActorLocation();
	const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	// Collapse unused rows to the oldest sample so the same mesh section can be updated in place.
	const int32 Padding = MaxSamples - Samples.Num();
	for (int32 Row = 0; Row < MaxSamples; ++Row)
	{
		const int32 Index = FMath::Max(0, Row - Padding);
		const FSample& Point = Samples[Index];
		const float Age = FMath::Clamp((Now - Point.Time) / FadeTime, 0., 1.);
		const float Life = FMath::Square(1.f - Age);
		const FVector Path = Samples[FMath::Min(Index + 1, Samples.Num() - 1)].Tip - Samples[FMath::Max(Index - 1, 0)].Tip;
		const FVector View = Camera ? Camera->GetCameraLocation() - Point.Tip : FVector::UpVector;
		const FVector Edge = FVector::CrossProduct(Path, View).GetSafeNormal() * (TipWidth * .5f);
		// UV.x = 0..1 for the blade sheet; 2..3 for the narrow tip streak that also reads on a thrust.
		const FVector Positions[] = {Point.Root, Point.Tip, Point.Tip - Edge, Point.Tip + Edge};
		constexpr float Across[] = {0.f, 1.f, 2.f, 3.f};
		for (int32 I = 0; I < VerticesPerSample; ++I)
		{
			Vertices.Add(Positions[I] - Origin);
			UVs.Emplace(Across[I], Age);
			Colors.Add(FLinearColor(1.f, 1.f, 1.f, Life));
		}
	}
	if (Ribbon->GetNumSections() == 0)
		Ribbon->CreateMeshSection_LinearColor(0, Vertices, Indices, {}, UVs, Colors, {}, false);
	else
		Ribbon->UpdateMeshSection_LinearColor(0, Vertices, {}, UVs, Colors, {}, false);
	Ribbon->SetVisibility(true);
}
