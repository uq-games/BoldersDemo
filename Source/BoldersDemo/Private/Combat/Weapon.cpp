#include "Combat/Weapon.h"
#include "Combat/AnimNotifyState_WeaponTrail.h"
#include "WarriorBladeTrail.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
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
