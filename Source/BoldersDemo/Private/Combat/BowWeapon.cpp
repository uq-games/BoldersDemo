#include "Combat/BowWeapon.h"
#include "Characters/BaseCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABowWeapon::ABowWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	UpperString = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperString"));
	LowerString = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerString"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	for (auto* Part : {UpperString.Get(), LowerString.Get()})
	{
		Part->SetupAttachment(Mesh);
		Part->SetStaticMesh(Cylinder.Object);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
	}
}

void ABowWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* Character = Cast<ABaseCharacter>(GetOwner());
	if (!Character) return;
	const auto* CharacterMesh = Character->GetMesh();
	const auto* Animation = CharacterMesh->GetAnimInstance();
	const FTransform BowTransform = Mesh->GetComponentTransform();
	const FVector Top = BowTransform.TransformPosition(FVector(-35., 98.5, 0.));
	const FVector Bottom = BowTransform.TransformPosition(FVector(-35., -98.5, 0.));
	const float Draw = Animation ? FMath::Clamp(Animation->GetCurveValue(TEXT("BowDraw")), 0.f, 1.f) : 0.f;
	const FVector Nock = FMath::Lerp((Top + Bottom) * .5, CharacterMesh->GetSocketLocation(TEXT("handslot_r")), Draw);
	const double Thickness = .006 * BowTransform.GetScale3D().GetAbsMax();
	auto Place = [Thickness](UStaticMeshComponent* Part, const FVector& A, const FVector& B)
	{
		const FVector Delta = B - A;
		Part->SetWorldLocation((A + B) * .5);
		Part->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
		Part->SetWorldScale3D(FVector(Thickness, Thickness, Delta.Length() / 100.));
	};
	Place(UpperString, Top, Nock);
	Place(LowerString, Nock, Bottom);
}
