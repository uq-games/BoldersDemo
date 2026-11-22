#include "Combat/Abilities/BasicAttackAbility.h"
#include "AI/Positioning/AttackPositioningComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Characters/BaseCharacter.h"
#include "Combat/Abilities/AnimNotify_AttackHit.h"
#include "AI/Positioning/AttackPositionRules.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Equipment/Weapon.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/Attacks/AttackProjectile.h"
#include "Combat/Attacks/AttackResolution.h"
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
	float StartTime = 0.f;
	if (const FAttackModule* Module = GetModule())
	{
		// Keep the final preparation and contact poses at their authored speed.
		const auto* Release = Montage->Notifies.FindByPredicate([](const FAnimNotifyEvent& Event)
		{
			return Event.Notify && Event.Notify->IsA<UAnimNotify_AttackHit>();
		});
		const float Rate = FMath::Max(Montage->RateScale, UE_SMALL_NUMBER);
		const float Lead = Module->Pattern == EAttackPattern::SingleMelee ? .24f : .30f;
		const float ReleaseTime = Release ? Release->GetTriggerTime() : 0.f;
		StartTime = FMath::Max(0.f, ReleaseTime - Lead * Rate);
		if (Module->Pattern == EAttackPattern::SkyStrike)
			SkyStrikeAim = AttackResolution::GroundPoint(*Target.Get());
		Attacker->MulticastModuleWindup(ModuleId, (ReleaseTime - StartTime) / Rate + .05f, SkyStrikeAim);
	}
	auto* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_Event_AttackHit, nullptr, true, true);
	EventTask->EventReceived.AddDynamic(this, &ThisClass::OnHitEvent);
	EventTask->ReadyForActivation();
	auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage,
		1.f, NAME_None, true, 1.f, StartTime);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageComplete);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();
}
void UBasicAttackAbility::OnHitEvent(FGameplayEventData Payload)
{
	if (!bHitPending) return;
	bHitPending = false;
	const FAttackModule* Module = GetModule();
	if (Module) Attacker->MulticastModuleWindup(ModuleId, 0.f, FVector::ZeroVector);
	if (!IsTargetInReach()) return;
	if (Module && (Module->Pattern == EAttackPattern::SingleMelee || Module->Pattern == EAttackPattern::Cleave))
	{
		AttackResolution::ReleaseMelee(*Attacker.Get(), *Target.Get(), *Module);
		return;
	}
	if (Module && Module->Pattern == EAttackPattern::SkyStrike)
	{
		AttackResolution::ReleaseSkyStrike(*Attacker.Get(), *Module, SkyStrikeAim);
		return;
	}
	const TSubclassOf<AAttackProjectile> ProjectileClass = Module ? Module->ProjectileClass : Weapon->ProjectileClass;
	if (!ProjectileClass)
	{
		if (Module) return;
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
	if (bHitPending && Attacker.IsValid() && Attacker->HasAuthority() && GetModule())
		Attacker->MulticastModuleWindup(ModuleId, 0.f, FVector::ZeroVector);
	bHitPending = false;
	if (Attacker.IsValid() && Attacker->HasAuthority()) Attacker->AttackPositioning->Cancel();
	Attacker.Reset(); Target.Reset(); Weapon.Reset();
	ModuleId = NAME_None;
	SkyStrikeAim = FVector::ZeroVector;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
