#include "Combat/CharacterCombatComponent.h"
#include "Combat/Abilities/BasicAttackAbility.h"
#include "Combat/Abilities/CharacterAbilityData.h"
#include "Combat/Equipment/Weapon.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "AI/Positioning/AttackPositionRules.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/BoldersPlayerController.h"
#include "TimerManager.h"

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
	if (Weapon)
	{
		Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->AttachSocket);
	}
}

void UCharacterCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCharacterCombatComponent, Weapon);
	DOREPLIFETIME(UCharacterCombatComponent, ActiveModuleId);
	DOREPLIFETIME(UCharacterCombatComponent, ReadyTimes);
	DOREPLIFETIME(UCharacterCombatComponent, OrderPresentation);
	DOREPLIFETIME(UCharacterCombatComponent, BladeCoating);
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

EAttackOrderPhase UCharacterCombatComponent::GetOrderPhase() const
{
	const EAttackOrderPhase Phase = OrderPresentation.Phase;
	if (Phase == EAttackOrderPhase::Approaching || Phase == EAttackOrderPhase::Windup) return Phase;
	return GetCombatTime() - OrderPresentation.ChangedAt < 1.4f ? Phase : EAttackOrderPhase::Idle;
}

void UCharacterCombatComponent::SetOrderPhase(EAttackOrderPhase Phase)
{
	if (!Character || !Character->HasAuthority()) return;
	OrderPresentation = {Phase, GetCombatTime()};
	Character->ForceNetUpdate();
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
	if (!Character) return;
	if (Weapon && Weapon->Mesh)
	{
		const AWeapon* Defaults = Weapon->GetClass()->GetDefaultObject<AWeapon>();
		Weapon->Mesh->SetRelativeScale3D(Defaults->Mesh->GetRelativeScale3D() * Character->GetCombatModelScale());
	}
	const FAttackModule* Module = GetActiveModule();
	if (!Module) return;
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
	OnRep_BladeCoating();
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
	GetWorld()->GetTimerManager().ClearTimer(BladeCoatingTimer);
	CancelAttack();
	if (Character && Character->HasAuthority() && BasicAttackHandle.IsValid())
		Character->GetAbilitySystemComponent()->ClearAbility(BasicAttackHandle);
	BasicAttackHandle = FGameplayAbilitySpecHandle();
	if (Character && Character->HasAuthority() && IsValid(Weapon)) Weapon->Destroy();
	if (IsValid(Offhand)) Offhand->DestroyComponent();
	Super::EndPlay(Reason);
}
EAttackOrderResult UCharacterCombatComponent::RequestAttack(AActor* Target, FName ModuleId)
{
	if (!Character || !Character->HasAuthority() || !Character->IsAlive()
		|| !IsValid(Weapon) || !BasicAttackHandle.IsValid()) return EAttackOrderResult::Unavailable;
	if (Character->IsFrozen()) return EAttackOrderResult::Frozen;
	if (!AttackPositionRules::CanTarget(*Character, Target)) return EAttackOrderResult::InvalidTarget;
	const FAttackModule* Requested = ModuleId.IsNone() ? GetActiveModule() : FindModule(ModuleId);
	if (!ModuleId.IsNone() && !Requested) return EAttackOrderResult::Unavailable;
	if (const auto* Hazard = Cast<AVolcanoHazard>(Target); Hazard && !Hazard->CanReactTo(GetEffectiveAttackElement(Requested)))
		return EAttackOrderResult::Ineffective;
	if (!ModuleId.IsNone() && !EquipAttack(ModuleId)) return EAttackOrderResult::Unavailable;
	const FAttackModule* Module = GetActiveModule();
	if (Module ? Module->Montages.IsEmpty() : Weapon->AttackMontages.IsEmpty()) return EAttackOrderResult::Unavailable;
	if (Character->HasDirectCombatInput())
	{
		const float Range = Module ? Module->Range : Weapon->AttackRange;
		const FVector Origin = Character->GetActorLocation();
		if (!AttackPositionRules::IsInRange(*Character, *Target, Range, Origin))
			return EAttackOrderResult::OutOfRange;
		if (!AttackPositionRules::CanAttack(*Character, *Target, Range, Origin))
			return EAttackOrderResult::Obstructed;
	}

	const auto* Spec = Character->GetAbilitySystemComponent()->FindAbilitySpecFromHandle(BasicAttackHandle);
	const auto Phase = OrderPresentation.Phase;
	if (OrderedTarget.Get() == Target && Spec && Spec->IsActive()
		&& (Phase == EAttackOrderPhase::Approaching || Phase == EAttackOrderPhase::Windup))
		return EAttackOrderResult::Accepted;

	const float Remaining = Module ? GetCooldownRemaining(Module->Id) : 0.f;
	if (Remaining > .44f) return EAttackOrderResult::CoolingDown;
	if (Remaining > 0.f)
	{
		BufferedTarget = Target;
		GetWorld()->GetTimerManager().SetTimer(BufferedAttackTimer, this, &ThisClass::RunBufferedAttack, Remaining + .02f, false);
		return EAttackOrderResult::Buffered;
	}

	CancelAttack();
	OrderedTarget = Target;
	FGameplayEventData Payload; Payload.Instigator = Character; Payload.Target = Target;
	return Character->GetAbilitySystemComponent()->HandleGameplayEvent(TAG_Event_BasicAttack, &Payload) > 0
		? EAttackOrderResult::Accepted : EAttackOrderResult::Unavailable;
}

void UCharacterCombatComponent::RunBufferedAttack()
{
	AActor* Target = BufferedTarget.Get();
	BufferedTarget.Reset();
	ReportAttackOrderResult(IsValid(Target) ? RequestAttack(Target) : EAttackOrderResult::InvalidTarget);
}

void UCharacterCombatComponent::ReportAttackOrderResult(EAttackOrderResult Result) const
{
	if (!Character || !Character->HasAuthority() || Character->PlayerSlot <= 0) return;
	if (auto* Controller = Cast<ABoldersPlayerController>(Character->GetController()))
	{
		if (Controller->GetAssignedCharacter() == Character) Controller->ClientAttackOrderFeedback(Result);
		return;
	}
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* Controller = Cast<ABoldersPlayerController>(It->Get());
		if (Controller && Controller->GetAssignedCharacter() == Character)
		{
			Controller->ClientAttackOrderFeedback(Result);
			return;
		}
	}
}

void UCharacterCombatComponent::CancelAttack()
{
	if (!Character || !Character->HasAuthority()) return;
	GetWorld()->GetTimerManager().ClearTimer(BufferedAttackTimer);
	BufferedTarget.Reset();
	OrderedTarget.Reset();
	if (BasicAttackHandle.IsValid()) Character->GetAbilitySystemComponent()->CancelAbilityHandle(BasicAttackHandle);
	if (OrderPresentation.Phase != EAttackOrderPhase::Idle) SetOrderPhase(EAttackOrderPhase::Idle);
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
