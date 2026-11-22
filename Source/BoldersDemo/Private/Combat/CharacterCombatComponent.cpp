#include "Combat/CharacterCombatComponent.h"
#include "Combat/Abilities/BasicAttackAbility.h"
#include "Combat/Abilities/CharacterAbilityData.h"
#include "Combat/Equipment/Weapon.h"
#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UCharacterCombatComponent::UCharacterCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
void UCharacterCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = CastChecked<ABaseCharacter>(GetOwner());
	SpawnWeapon();
	if (Character->HasAuthority() && AttackLoadout && !AttackLoadout->Modules.IsEmpty())
	{
		ReadyTimes.SetNumZeroed(AttackLoadout->Modules.Num());
		ActiveModuleId = AttackLoadout->Modules[0].Id;
	}
	OnRep_Equipment();
}
void UCharacterCombatComponent::SpawnWeapon()
{
	if (!Character->HasAuthority() || !WeaponClass) return;
	FActorSpawnParameters Params; Params.Owner = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, Params);
	if (Weapon) Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->AttachSocket);
}

void UCharacterCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCharacterCombatComponent, Weapon);
	DOREPLIFETIME(UCharacterCombatComponent, ActiveModuleId);
	DOREPLIFETIME(UCharacterCombatComponent, ReadyTimes);
}

const FAttackModule* UCharacterCombatComponent::FindModule(FName Id) const
{
	return AttackLoadout ? AttackLoadout->Modules.FindByPredicate([Id](const FAttackModule& Entry) { return Entry.Id == Id; }) : nullptr;
}

float UCharacterCombatComponent::GetCombatTime() const
{
	const auto* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float UCharacterCombatComponent::GetCooldownRemaining(FName Id) const
{
	const FAttackModule* Module = FindModule(Id);
	if (!Module) return 0.f;
	const int32 Index = static_cast<int32>(Module - AttackLoadout->Modules.GetData());
	return ReadyTimes.IsValidIndex(Index) ? FMath::Max(0.f, ReadyTimes[Index] - GetCombatTime()) : 0.f;
}

void UCharacterCombatComponent::CommitAttackCooldown()
{
	const FAttackModule* Module = GetActiveModule();
	if (!Module || !Character->HasAuthority()) return;
	ReadyTimes[Module - AttackLoadout->Modules.GetData()] = GetCombatTime() + Module->Cooldown;
	Character->ForceNetUpdate();
}

bool UCharacterCombatComponent::EquipAttack(FName ModuleId)
{
	if (!Character || !Character->HasAuthority() || !Character->IsAlive() || !FindModule(ModuleId)) return false;
	if (ActiveModuleId == ModuleId) return true;
	CancelAttack();
	ActiveModuleId = ModuleId;
	AttackIndex = 0;
	OnRep_Equipment();
	Character->ForceNetUpdate();
	return true;
}

void UCharacterCombatComponent::OnRep_Equipment()
{
	const FAttackModule* Module = GetActiveModule();
	if (!Character || !Module) return;
	if (Weapon && Module->WeaponMesh)
	{
		Weapon->Mesh->SetStaticMesh(Module->WeaponMesh);
		Weapon->Mesh->SetRelativeLocation(Module->WeaponGripOffset);
	}
	if (!Offhand && Module->OffhandMesh)
	{
		Offhand = NewObject<UStaticMeshComponent>(Character, TEXT("AttackOffhand"));
		Offhand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Offhand->SetCanEverAffectNavigation(false);
		Offhand->RegisterComponent();
	}
	if (Offhand)
	{
		Offhand->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, Module->OffhandSocket);
		Offhand->SetStaticMesh(Module->OffhandMesh);
	}
}
void UCharacterCombatComponent::GrantBasicAttack()
{
	if (!Character || !Character->HasAuthority() || !Weapon || BasicAttackHandle.IsValid()) return;
	if (!AbilityData || !AbilityData->BasicAttack || AbilityData->BasicAttackLevel < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicAttack: %s has a weapon but invalid ability data"), *GetNameSafe(Character));
		return;
	}
	BasicAttackHandle = Character->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(AbilityData->BasicAttack, AbilityData->BasicAttackLevel));
	UE_LOG(LogTemp, Log, TEXT("BasicAttack: %s granted %s level=%d from %s"),
		*GetNameSafe(Character), *GetNameSafe(AbilityData->BasicAttack.Get()), AbilityData->BasicAttackLevel, *GetNameSafe(AbilityData));
}
void UCharacterCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelAttack();
	if (Character && Character->HasAuthority() && BasicAttackHandle.IsValid())
		Character->GetAbilitySystemComponent()->ClearAbility(BasicAttackHandle);
	BasicAttackHandle = FGameplayAbilitySpecHandle();
	if (Character && Character->HasAuthority() && IsValid(Weapon)) Weapon->Destroy();
	if (IsValid(Offhand)) Offhand->DestroyComponent();
	Super::EndPlay(Reason);
}
bool UCharacterCombatComponent::RequestAttack(ABaseCharacter* Target, FName ModuleId)
{
	if (!Character || !Character->HasAuthority() || !Character->IsAlive() || Character->IsFrozen()
		|| !IsValid(Target) || !Target->IsAlive() || !Character->IsHostileTo(Target)
		|| !IsValid(Weapon) || !BasicAttackHandle.IsValid()) return false;
	if (!ModuleId.IsNone() && !EquipAttack(ModuleId)) return false;
	const FAttackModule* Module = GetActiveModule();
	if (Module ? Module->Montages.IsEmpty() || GetCooldownRemaining(Module->Id) > 0.f : Weapon->AttackMontages.IsEmpty()) return false;
	CancelAttack();
	FGameplayEventData Payload; Payload.Instigator = Character; Payload.Target = Target;
	return Character->GetAbilitySystemComponent()->HandleGameplayEvent(TAG_Event_BasicAttack, &Payload) > 0;
}
void UCharacterCombatComponent::CancelAttack()
{
	if (Character && Character->HasAuthority() && BasicAttackHandle.IsValid())
		Character->GetAbilitySystemComponent()->CancelAbilityHandle(BasicAttackHandle);
}
UAnimMontage* UCharacterCombatComponent::SelectNextMontage()
{
	if (!Weapon) return nullptr;
	const FAttackModule* Module = GetActiveModule();
	const auto& Montages = Module ? Module->Montages : Weapon->AttackMontages;
	if (Montages.IsEmpty()) return nullptr;
	AttackIndex %= Montages.Num();
	return Montages[AttackIndex++];
}
