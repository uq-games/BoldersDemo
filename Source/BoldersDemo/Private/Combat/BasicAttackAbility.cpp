#include "Combat/BasicAttackAbility.h"
#include "AI/AttackPositioningComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Characters/BaseCharacter.h"
#include "Combat/AttackPositionRules.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Weapon.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/AttackProjectile.h"
#include "Combat/AttackResolution.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Event_BasicAttack, "Event.Ability.BasicAttack");
UE_DEFINE_GAMEPLAY_TAG(TAG_Event_AttackHit, "Event.Montage.AttackHit");

UBasicAttackAbility::UBasicAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = TAG_Event_BasicAttack;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}
void UBasicAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	Attacker = Cast<ABaseCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	Target = Cast<ABaseCharacter>(TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr);
	Weapon = Attacker.IsValid() ? Attacker->Combat->GetWeapon() : nullptr;
	ModuleId = Attacker.IsValid() ? Attacker->Combat->GetActiveModuleId() : NAME_None;
	if (!Attacker.IsValid() || !Attacker->HasAuthority() || !Attacker->IsAlive() || Attacker->IsFrozen() || !Target.IsValid()
		|| !Target->IsAlive() || !Attacker->IsHostileTo(Target.Get()) || !Weapon.IsValid()
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo)) { Finish(true); return; }
	if (auto* AI = Cast<AAIController>(Attacker->GetController())) AI->StopMovement();
	if (IsTargetInReach()) { StartMontage(); return; }
	const auto Callback = FOnAttackPositionReady::CreateUObject(this, &ThisClass::OnPositionReady);
	if (!Attacker->AttackPositioning->RequestPosition(Target.Get(), GetAttackRange(),
		Attacker->Combat->AttackPositionQuery, Callback)) Finish(true);
}

const FAttackModule* UBasicAttackAbility::GetModule() const
{
	return Attacker.IsValid() ? Attacker->Combat->FindModule(ModuleId) : nullptr;
}

float UBasicAttackAbility::GetAttackRange() const
{
	const FAttackModule* Module = GetModule();
	return Module ? Module->Range : Weapon->AttackRange;
}

bool UBasicAttackAbility::IsTargetInReach() const
{
	return Attacker.IsValid() && Target.IsValid() && Weapon.IsValid() && Attacker->IsAlive()
		&& !Attacker->IsFrozen()
		&& Attacker->IsHostileTo(Target.Get())
		&& AttackPositionRules::CanAttack(*Attacker, *Target, GetAttackRange(), Attacker->GetActorLocation());
}
void UBasicAttackAbility::OnPositionReady(bool bArrived)
{
	if (bArrived) StartMontage(); else Finish(true);
}
void UBasicAttackAbility::StartMontage()
{
	if (!IsTargetInReach()) { Finish(true); return; }
	UAnimMontage* Montage = Attacker->Combat->SelectNextMontage();
	if (!Montage) { Finish(true); return; }
	FVector Direction = Target->GetActorLocation() - Attacker->GetActorLocation(); Direction.Z = 0;
	Attacker->SetActorRotation(Direction.Rotation());
	Attacker->Combat->CommitAttackCooldown();
	bHitPending = true;
	if (const FAttackModule* Module = GetModule(); Module && Module->Windup)
		Attacker->MulticastModuleWindup(ModuleId, FMath::Clamp(Montage->GetPlayLength(), .45f, 3.f));
	auto* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_AttackHit, nullptr, true, true);
	EventTask->EventReceived.AddDynamic(this, &ThisClass::OnHitEvent);
	EventTask->ReadyForActivation();
	auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageComplete);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}
void UBasicAttackAbility::OnHitEvent(FGameplayEventData Payload)
{
	if (!bHitPending) return;
	bHitPending = false;
	if (const FAttackModule* Module = GetModule(); Module && Module->Windup)
		Attacker->MulticastModuleWindup(ModuleId, 0.f);
	if (!IsTargetInReach()) return;
	const FAttackModule* Module = GetModule();
	if (Module && (Module->Pattern == EAttackPattern::SingleMelee || Module->Pattern == EAttackPattern::Cleave))
	{
		AttackResolution::ReleaseMelee(*Attacker.Get(), *Target.Get(), *Module);
		return;
	}
	if (Module && Module->Pattern == EAttackPattern::SkyStrike)
	{
		AttackResolution::WarnSkyStrike(*Attacker.Get(), *Target.Get(), *Module);
		return;
	}
	const TSubclassOf<AAttackProjectile> ProjectileClass = Module ? Module->ProjectileClass : Weapon->ProjectileClass;
	if (!ProjectileClass)
	{
		if (Module) return;
		// Centre the sweep at the animated weapon hand so crouching and turning move the arc with the blade.
		Attacker->MulticastAttackVisual(Weapon->Mesh->GetComponentLocation(), Attacker->GetActorRotation(), false);
		Target->ReceiveDamageFrom(Attacker.Get(), Weapon->Damage);
		return;
	}
	FVector Start = Weapon->bUseWeaponReleasePoint
		? Weapon->Mesh->GetComponentTransform().TransformPosition(Weapon->WeaponReleasePoint)
		: Attacker->GetActorLocation() + Attacker->GetActorRotation().RotateVector(Weapon->ProjectileSpawnOffset);
	FVector Direction = (Target->GetActorLocation() - Start).GetSafeNormal();
	if (Module && Module->Pattern == EAttackPattern::Wave)
	{
		Direction = (Target->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
		Start = AttackResolution::GroundPoint(*Attacker.Get()) + Direction * 42.f + FVector(0, 0, 42.f);
	}
	// Reject a muzzle placed across a thin obstacle instead of spawning through it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectileMuzzle), false, Attacker.Get());
	Params.AddIgnoredActor(Weapon.Get());
	Params.AddIgnoredActor(Target.Get());
	if (GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation(), Start, ECC_Visibility, Params)) return;
	const FTransform Transform(Direction.Rotation(), Start);
	auto* Projectile = GetWorld()->SpawnActorDeferred<AAttackProjectile>(ProjectileClass, Transform,
		Attacker.Get(), Attacker.Get(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		if (Module)
		{
			Projectile->InitializeModuleShot(Attacker.Get(), *Module, Direction, Module->Damage);
			Attacker->MulticastModuleVisual(ModuleId, EAttackVisualStage::Cast, Start, Start + Direction * 100.f);
		}
		else Projectile->InitializeShot(Attacker.Get(), Weapon->Damage, Direction);
		Projectile->FinishSpawning(Transform);
	}
}
void UBasicAttackAbility::OnMontageComplete() { Finish(false); }
void UBasicAttackAbility::OnMontageInterrupted() { Finish(true); }
void UBasicAttackAbility::Finish(bool bCancelled)
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}
void UBasicAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bHitPending && Attacker.IsValid() && Attacker->HasAuthority())
		if (const FAttackModule* Module = GetModule(); Module && Module->Windup)
			Attacker->MulticastModuleWindup(ModuleId, 0.f);
	bHitPending = false;
	if (Attacker.IsValid() && Attacker->HasAuthority()) Attacker->AttackPositioning->Cancel();
	Attacker.Reset(); Target.Reset(); Weapon.Reset();
	ModuleId = NAME_None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
