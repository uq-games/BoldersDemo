#include "Combat/CharacterCombatComponent.h"
#include "Combat/BasicAttackAbility.h"
#include "Combat/CharacterAbilityData.h"
#include "Combat/Weapon.h"
#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

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
}
void UCharacterCombatComponent::SpawnWeapon()
{
	if (!Character->HasAuthority() || !WeaponClass) return;
	FActorSpawnParameters Params; Params.Owner = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass, Params);
	if (Weapon) Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->AttachSocket);
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
	if (IsValid(Weapon)) Weapon->Destroy();
	Super::EndPlay(Reason);
}
bool UCharacterCombatComponent::RequestAttack(ABaseCharacter* Target)
{
	if (!Character || !Character->HasAuthority() || !Character->IsAlive()
		|| !IsValid(Target) || !Target->IsAlive() || !Character->IsHostileTo(Target)
		|| !IsValid(Weapon) || Weapon->AttackMontages.IsEmpty() || !BasicAttackHandle.IsValid()) return false;
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
	if (!Weapon || Weapon->AttackMontages.IsEmpty()) return nullptr;
	AttackIndex %= Weapon->AttackMontages.Num();
	return Weapon->AttackMontages[AttackIndex++];
}
