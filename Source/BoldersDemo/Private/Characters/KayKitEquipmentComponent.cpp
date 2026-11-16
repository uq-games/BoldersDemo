#include "Characters/KayKitEquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

UKayKitEquipmentComponent::UKayKitEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sword(TEXT("/Game/BoldersDemo/Assets/Characters/Warrior/Accessories/SM_sword_1handed"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shield(TEXT("/Game/BoldersDemo/Assets/Characters/Warrior/Accessories/SM_shield_badge_color"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BowMesh(TEXT("/Game/BoldersDemo/Assets/Characters/Archer/Accessories/SM_bow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Staff(TEXT("/Game/BoldersDemo/Assets/Characters/Mage/Accessories/SM_staff"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	SwordAsset = Sword.Object;
	ShieldAsset = Shield.Object;
	BowAsset = BowMesh.Object;
	StaffAsset = Staff.Object;
	StringAsset = Cylinder.Object;
}

USkeletalMeshComponent* UKayKitEquipmentComponent::GetCharacterMesh() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetMesh() : nullptr;
}

void UKayKitEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (USkeletalMeshComponent* Mesh = GetCharacterMesh())
	{
		AddTickPrerequisiteComponent(Mesh);
	}
	RefreshEquipment();
}

void UKayKitEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkeletalMeshComponent* Mesh = GetCharacterMesh())
	{
		RemoveTickPrerequisiteComponent(Mesh);
	}
	ClearEquipment();
	Super::EndPlay(EndPlayReason);
}

void UKayKitEquipmentComponent::ClearEquipment()
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (IsValid(Part)) Part->DestroyComponent();
	}
	Parts.Reset();
	Bow = nullptr;
	UpperString = nullptr;
	LowerString = nullptr;
	EquippedMesh.Reset();
	StringPoints.Reset();
}

UStaticMeshComponent* UKayKitEquipmentComponent::MakePart(UStaticMesh* Asset, FName Socket)
{
	USkeletalMeshComponent* Mesh = GetCharacterMesh();
	if (!Asset || !Mesh || !Mesh->DoesSocketExist(Socket)) return nullptr;
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Parts.Add(Part);
	Part->SetStaticMesh(Asset);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetupAttachment(Mesh, Socket);
	Part->RegisterComponent();
	Part->SetRelativeTransform(FTransform::Identity);
	return Part;
}

void UKayKitEquipmentComponent::RefreshEquipment()
{
	ClearEquipment();
	USkeletalMeshComponent* Mesh = GetCharacterMesh();
	if (!Mesh || GetNetMode() == NM_DedicatedServer) return;
	EquippedMesh = Mesh->GetSkeletalMeshAsset();
	if (!EquippedMesh.IsValid()) return;
	const FString Path = EquippedMesh->GetPathName();
	if (Path == TEXT("/Game/BoldersDemo/Assets/Characters/Warrior/SK_Warrior.SK_Warrior"))
	{
		MakePart(SwordAsset, TEXT("Weapon_R"));
		MakePart(ShieldAsset, TEXT("Shield_L"));
	}
	else if (Path == TEXT("/Game/BoldersDemo/Assets/Characters/Mage/SK_Mage.SK_Mage"))
	{
		MakePart(StaffAsset, TEXT("Staff_R"));
	}
	else if (Path == TEXT("/Game/BoldersDemo/Assets/Characters/Archer/SK_Archer.SK_Archer"))
	{
		Bow = MakePart(BowAsset, TEXT("Bow_L"));
		UpperString = MakePart(StringAsset, TEXT("Bow_L"));
		LowerString = MakePart(StringAsset, TEXT("Bow_L"));
		if (UpperString) UpperString->SetCastShadow(false);
		if (LowerString) LowerString->SetCastShadow(false);
		UpdateBowString();
	}
}

void UKayKitEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	USkeletalMeshComponent* Mesh = GetCharacterMesh();
	if (Mesh && EquippedMesh.Get() != Mesh->GetSkeletalMeshAsset()) RefreshEquipment();
	UpdateBowString();
}

void UKayKitEquipmentComponent::UpdateBowString()
{
	USkeletalMeshComponent* Mesh = GetCharacterMesh();
	if (!Mesh || !Bow || !UpperString || !LowerString) return;
	const FTransform BowTransform = Bow->GetComponentTransform();
	// Endpoints measured on the source bow mesh in UE local centimetres.
	const FVector Top = BowTransform.TransformPosition(FVector(-35.0, 98.5, 0.0));
	const FVector Bottom = BowTransform.TransformPosition(FVector(-35.0, -98.5, 0.0));
	const FVector Rest = (Top + Bottom) * 0.5;
	const UAnimInstance* Anim = Mesh->GetAnimInstance();
	const float Draw = Anim ? FMath::Clamp(Anim->GetCurveValue(TEXT("BowDraw")), 0.0f, 1.0f) : 0.0f;
	const FVector Nock = FMath::Lerp(Rest, Mesh->GetSocketLocation(TEXT("handslot_r")), Draw);
	StringPoints = {Top, Nock, Bottom};
	const double RadiusScale = 0.006 * BowTransform.GetScale3D().GetAbsMax();
	auto PlaceSegment = [RadiusScale, Mesh](UStaticMeshComponent* Part, const FVector& A, const FVector& B)
	{
		const FVector Delta = B - A;
		Part->SetWorldLocationAndRotation((A + B) * 0.5, FRotationMatrix::MakeFromZ(Delta).ToQuat());
		// The engine cylinder has radius 50 and length 100.
		Part->SetWorldScale3D(FVector(RadiusScale, RadiusScale, Delta.Size() / 100.0));
		Part->SetVisibility(Mesh->IsVisible() && Delta.SizeSquared() > UE_SMALL_NUMBER);
	};
	PlaceSegment(UpperString, Top, Nock);
	PlaceSegment(LowerString, Nock, Bottom);
}

TArray<FVector> UKayKitEquipmentComponent::GetBowStringPoints() const
{
	return StringPoints;
}
