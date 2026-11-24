#include "Combat/Equipment/Weapon.h"
#include "Animation/AnimMontage.h"
#include "Combat/Presentation/AnimNotifyState_WeaponTrail.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Combat/Presentation/WarriorBladeTrail.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetReceivesDecals(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> CoatingMaterial(CombatGuideStyle::MaterialPath);
	BladeCoatingMaterial = CoatingMaterial.Object;
}

void AWeapon::SetBladeCoating(EDamageElement Element, const FAttackModule* Module)
{
	DisplayedCoating = Element;
	if (BladeCoatingMesh) BladeCoatingMesh->SetVisibility(false);
	if (Element == EDamageElement::Physical || !BladeCoatingMaterial || !Module) return;

	const UAnimNotifyState_WeaponTrail* Blade = nullptr;
	for (const UAnimMontage* Montage : Module->Montages)
	{
		if (!Montage) continue;
		const auto* Notify = Montage->Notifies.FindByPredicate([](const FAnimNotifyEvent& Event)
		{
			return IsValid(Cast<UAnimNotifyState_WeaponTrail>(Event.NotifyStateClass));
		});
		if (Notify)
		{
			Blade = Cast<UAnimNotifyState_WeaponTrail>(Notify->NotifyStateClass);
			break;
		}
	}
	if (!Blade) return;

	if (!BladeCoatingMesh)
	{
		BladeCoatingMesh = NewObject<UProceduralMeshComponent>(this);
		BladeCoatingMesh->SetupAttachment(Mesh);
		BladeCoatingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BladeCoatingMesh->SetCanEverAffectNavigation(false);
		BladeCoatingMesh->SetCastShadow(false);
		BladeCoatingMesh->SetMaterial(0, BladeCoatingMaterial);
		BladeCoatingMesh->RegisterComponent();
	}

	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<FLinearColor> Colors;
	FVector Across, Depth;
	const FVector Axis = (Blade->BladeTip - Blade->BladeRoot).GetSafeNormal();
	Axis.FindBestAxisVectors(Across, Depth);
	const FLinearColor Color = CombatGuideStyle::ElementColor(Element);
	for (const FVector& Side : {Across, Depth})
	{
		const int32 First = Vertices.Num();
		const float Positions[] = {0.f, .17f, .79f, 1.06f};
		const float Widths[] = {4.f, 16.f, 13.f, 4.f};
		for (int32 Row = 0; Row < UE_ARRAY_COUNT(Positions); ++Row)
		{
			const FVector Center = FMath::Lerp(Blade->BladeRoot, Blade->BladeTip, Positions[Row]);
			Vertices.Append({Center - Side * Widths[Row], Center, Center + Side * Widths[Row]});
			const float Glow = Row == 0 || Row == 3 ? .25f : .87f;
			Colors.Append({Color.CopyWithNewOpacity(0.f), Color.CopyWithNewOpacity(Glow), Color.CopyWithNewOpacity(0.f)});
			if (Row == 0) continue;
			for (int32 Column = 0; Column < 2; ++Column)
			{
				const int32 A = First + (Row - 1) * 3 + Column;
				Indices.Append({A, A + 3, A + 1, A + 1, A + 3, A + 4});
			}
		}
	}
	BladeCoatingMesh->CreateMeshSection_LinearColor(0, Vertices, Indices, {}, {}, Colors, {}, false, false);
	BladeCoatingMesh->SetVisibility(true);
}

void AWeapon::StartBladeTrail(USkeletalMeshComponent& Pose, const UAnimNotifyState_WeaponTrail& Style)
{
	if (!Style.TrailMaterial || !Mesh || !Mesh->GetStaticMesh() || !GetWorld()) return;
	if (BladeTrail.IsValid()) BladeTrail->Destroy();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BladeTrail = GetWorld()->SpawnActor<AWarriorBladeTrail>(Mesh->GetComponentLocation(), FRotator::ZeroRotator, Params);
	if (BladeTrail.IsValid()) BladeTrail->Initialize(*this, Pose, Style);
}

void AWeapon::StopBladeTrail(const UAnimNotifyState_WeaponTrail& Style)
{
	if (BladeTrail.IsValid()) BladeTrail->Stop(Style);
}
