#include "Combat/Abilities/BasicAttackAbility.h"
#include "AI/Positioning/AttackPositioningComponent.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Characters/BaseCharacter.h"
#include "Combat/Abilities/AnimNotify_AttackHit.h"
#include "AI/Positioning/AttackPositionRules.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Equipment/Weapon.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Combat/Environment/CombatCollision.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/Attacks/AttackProjectile.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

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
	Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	Weapon = Attacker.IsValid() ? Attacker->Combat->GetWeapon() : nullptr;
	ModuleId = Attacker.IsValid() ? Attacker->Combat->GetActiveModuleId() : NAME_None;
	if (!Attacker.IsValid() || !Attacker->HasAuthority() || !Attacker->IsAlive() || Attacker->IsFrozen() || !Target.IsValid()
		|| !AttackPositionRules::CanTarget(*Attacker.Get(), Target.Get()) || !Weapon.IsValid()
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo)) { Finish(true); return; }
	if (auto* AI = Cast<AAIController>(Attacker->GetController())) AI->StopMovement();
	if (IsTargetInReach()) { StartMontage(); return; }
	if (Attacker->HasDirectCombatInput()) { Finish(true); return; }
	Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Approaching);
	const auto Callback = FOnAttackPositionReady::CreateUObject(this, &ThisClass::OnPositionReady);
	if (!Attacker->AttackPositioning->RequestPosition(Target.Get(), GetAttackRange(),
		Attacker->Combat->AttackPositionQuery, Callback)) OnPositionReady(false);
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

bool UBasicAttackAbility::IsTargetInReach(float ContactTolerance) const
{
	return Attacker.IsValid() && Target.IsValid() && Weapon.IsValid() && Attacker->IsAlive()
		&& !Attacker->IsFrozen()
		&& AttackPositionRules::CanAttack(*Attacker, *Target, GetAttackRange() + ContactTolerance, Attacker->GetActorLocation());
}
void UBasicAttackAbility::OnPositionReady(bool bArrived)
{
	if (bArrived) { StartMontage(); return; }
	if (Attacker.IsValid()) Attacker->Combat->SetOrderPhase(EAttackOrderPhase::NoApproach);
	Finish(true);
}
void UBasicAttackAbility::StartMontage()
{
	if (!IsTargetInReach())
	{
		if (Attacker.IsValid()) Attacker->Combat->SetOrderPhase(EAttackOrderPhase::LostTarget);
		Finish(true);
		return;
	}
	UAnimMontage* Montage = Attacker->Combat->SelectNextMontage();
	if (!Montage) { Finish(true); return; }
	const FAttackModule* Attack = GetModule();
	if (const auto* Hazard = Cast<AVolcanoHazard>(Target.Get());
		Hazard && !Hazard->CanReactTo(Attacker->Combat->GetEffectiveAttackElement(Attack)))
	{
		Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Ineffective);
		Finish(true);
		return;
	}
	const bool bProjectile = Attack ? Attack->ProjectileClass != nullptr : Weapon->ProjectileClass != nullptr;
	FVector Start, DirectionAtRelease;
	if (bProjectile && !GetSafeMuzzle(Start, DirectionAtRelease))
	{
		Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Obstructed);
		Finish(true);
		return;
	}
	const bool bDirectInput = Attacker->HasDirectCombatInput();
	if (bDirectInput) Attacker->GetCharacterMovement()->StopMovementImmediately();
	const FVector Direction = (AttackPositionRules::GetAttackPoint(*Target.Get()) - Attacker->GetActorLocation()).GetSafeNormal2D();
	Attacker->SetActorRotation(Direction.Rotation());
	Attacker->Combat->CommitAttackCooldown();
	Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Windup);
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
		const bool bMelee = Module->Pattern == EAttackPattern::SingleMelee || Module->Pattern == EAttackPattern::Cleave;
		const float Lead = bDirectInput ? (bMelee ? .29f : .35f) : (Module->Pattern == EAttackPattern::SingleMelee ? .24f : .30f);
		const float ReleaseTime = Release ? Release->GetTriggerTime() : 0.f;
		StartTime = FMath::Max(0.f, ReleaseTime - Lead * Rate);
		if (Module->Pattern == EAttackPattern::SkyStrike)
		{
			const auto* Character = Cast<ABaseCharacter>(Target.Get());
			SkyStrikeAim = Character ? AttackResolution::GroundPoint(*Character) : Target->GetActorLocation() + FVector(0, 0, 3);
		}
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
	// GAS replicates this server-only montage to observers but skips a locally controlled avatar.
	// Send the same accepted montage and starting pose to its remote owner.
	if (IsActive() && Attacker.IsValid() && Attacker->GetController<APlayerController>() && !Attacker->IsLocallyControlled()
		&& Attacker->GetAbilitySystemComponent()->GetCurrentMontage() == Montage)
	{
		bOwnerMontageSent = true;
		Attacker->ClientPlayBasicAttackMontage(Montage, StartTime, Direction.Rotation());
	}
}
void UBasicAttackAbility::OnHitEvent(FGameplayEventData Payload)
{
	if (!bHitPending) return;
	bHitPending = false;
	if (!Attacker.IsValid()) return;
	const FAttackModule* Module = GetModule();
	if (Module) Attacker->MulticastModuleWindup(ModuleId, 0.f, FVector::ZeroVector);
	const bool bMelee = Module && (Module->Pattern == EAttackPattern::SingleMelee || Module->Pattern == EAttackPattern::Cleave);
	// Permit the remaining few centimetres of a committed swing as the enemy steps away.
	const float ContactTolerance = bMelee && Attacker->HasDirectCombatInput() ? 18.f : 0.f;
	const bool bInReach = IsTargetInReach(ContactTolerance);
	Attacker->Combat->SetOrderPhase(bInReach ? EAttackOrderPhase::Released : EAttackOrderPhase::LostTarget);
	if (!bInReach) return;
	auto* Hazard = Cast<AVolcanoHazard>(Target.Get());
	auto* Victim = Cast<ABaseCharacter>(Target.Get());
	if (bMelee)
	{
		AttackResolution::ReleaseMelee(*Attacker.Get(), *Target.Get(), *Module);
		return;
	}
	if (Module && Module->Pattern == EAttackPattern::SkyStrike)
	{
		AttackResolution::ReleaseSkyStrike(*Attacker.Get(), *Module, SkyStrikeAim, Hazard);
		return;
	}
	const TSubclassOf<AAttackProjectile> ProjectileClass = Module ? Module->ProjectileClass : Weapon->ProjectileClass;
	if (!ProjectileClass)
	{
		if (Module) return;
		if (Victim) Victim->ReceiveDamageFrom(Attacker.Get(), Weapon->Damage);
		if (Hazard) Hazard->ReceiveAttack(Attacker.Get(), EDamageElement::Physical);
		return;
	}
	FVector Start, Direction;
	if (!GetSafeMuzzle(Start, Direction))
	{
		Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Obstructed);
		return;
	}
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

bool UBasicAttackAbility::GetSafeMuzzle(FVector& Start, FVector& Direction) const
{
	const FVector Origin = Attacker->GetActorLocation();
	Start = Weapon->bUseWeaponReleasePoint
		? Weapon->Mesh->GetComponentTransform().TransformPosition(Weapon->WeaponReleasePoint)
		: Origin + Attacker->GetActorRotation().RotateVector(Weapon->ProjectileSpawnOffset);
	const FVector TargetPoint = AttackPositionRules::GetAttackPoint(*Target.Get());
	const FAttackModule* Module = GetModule();
	const bool bWave = Module && Module->Pattern == EAttackPattern::Wave;
	if (bWave)
	{
		Direction = (TargetPoint - Origin).GetSafeNormal2D();
		Start = AttackResolution::GroundPoint(*Attacker.Get()) + Direction * 42.f + FVector(0, 0, 42.f);
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectileMuzzle), false, Attacker.Get());
	Params.AddIgnoredActor(Weapon.Get());
	Params.AddIgnoredActor(Target.Get());
	const auto& Cover = BoldersCollision::SolidObjects();
	const FCollisionShape Shape = FCollisionShape::MakeSphere(bWave ? 14.f : 4.f);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, Origin, Start, FQuat::Identity, Cover, Shape, Params))
	{
		if (Hit.bStartPenetrating) return false;
		Start = Hit.Location - (Start - Origin).GetSafeNormal() * FMath::Min(4.f, Hit.Distance);
	}
	Direction = bWave ? (TargetPoint - Start).GetSafeNormal2D() : (TargetPoint - Start).GetSafeNormal();
	return !Direction.IsNearlyZero() && !GetWorld()->OverlapAnyTestByObjectType(Start, FQuat::Identity, Cover, Shape, Params);
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
	if (Attacker.IsValid() && Attacker->HasAuthority())
	{
		if (bOwnerMontageSent && bWasCancelled) Attacker->ClientStopBasicAttackMontage();
		Attacker->AttackPositioning->Cancel();
		const auto Phase = Attacker->Combat->GetOrderPhase();
		if (Phase == EAttackOrderPhase::Approaching || Phase == EAttackOrderPhase::Windup)
			Attacker->Combat->SetOrderPhase(EAttackOrderPhase::Idle);
	}
	bOwnerMontageSent = false;
	Attacker.Reset(); Target.Reset(); Weapon.Reset();
	ModuleId = NAME_None;
	SkyStrikeAim = FVector::ZeroVector;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
