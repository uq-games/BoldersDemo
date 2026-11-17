#include "Combat/BasicAttackAbility.h"
#include "AI/AttackPositioningComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/BaseCharacter.h"
#include "Combat/AttackPositionRules.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Weapon.h"
#include "Combat/AttackProjectile.h"
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
	if (!Attacker.IsValid() || !Attacker->HasAuthority() || !Attacker->IsAlive() || !Target.IsValid()
		|| !Target->IsAlive() || !Attacker->IsHostileTo(Target.Get()) || !Weapon.IsValid()
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo)) { Finish(true); return; }
	if (auto* AI = Cast<AAIController>(Attacker->GetController())) AI->StopMovement();
	if (IsTargetInReach()) { StartMontage(); return; }
	const auto Callback = FOnAttackPositionReady::CreateUObject(this, &ThisClass::OnPositionReady);
	if (!Attacker->AttackPositioning->RequestPosition(Target.Get(), Weapon->AttackRange,
		Attacker->Combat->AttackPositionQuery, Callback)) Finish(true);
}
bool UBasicAttackAbility::IsTargetInReach() const
{
	return Attacker.IsValid() && Target.IsValid() && Weapon.IsValid() && Attacker->IsAlive()
		&& Attacker->IsHostileTo(Target.Get())
		&& AttackPositionRules::CanAttack(*Attacker, *Target, Weapon->AttackRange, Attacker->GetActorLocation());
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
	bHitPending = true;
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
	if (!IsTargetInReach()) return;
	if (!Weapon->ProjectileClass)
	{
		Target->ReceiveDamageFrom(Attacker.Get(), Weapon->Damage);
		return;
	}
	const FVector Start = Attacker->GetActorLocation() + Attacker->GetActorRotation().RotateVector(Weapon->ProjectileSpawnOffset);
	const FVector Direction = (Target->GetActorLocation() - Start).GetSafeNormal();
	// Reject a muzzle placed across a thin obstacle instead of spawning through it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectileMuzzle), false, Attacker.Get());
	Params.AddIgnoredActor(Weapon.Get());
	Params.AddIgnoredActor(Target.Get());
	if (GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation(), Start, ECC_Visibility, Params)) return;
	const FTransform Transform(Direction.Rotation(), Start);
	auto* Projectile = GetWorld()->SpawnActorDeferred<AAttackProjectile>(Weapon->ProjectileClass, Transform,
		Attacker.Get(), Attacker.Get(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->InitializeShot(Attacker.Get(), Weapon->Damage, Direction);
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
	bHitPending = false;
	if (Attacker.IsValid() && Attacker->HasAuthority()) Attacker->AttackPositioning->Cancel();
	Attacker.Reset(); Target.Reset(); Weapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
