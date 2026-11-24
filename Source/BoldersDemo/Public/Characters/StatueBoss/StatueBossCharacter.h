#pragma once

#include "CoreMinimal.h"
#include "Characters/Roles/EnemyCharacter.h"
#include "Engine/NetSerialization.h"
#include "StatueBossCharacter.generated.h"

class UAnimSequence;
class UCapsuleComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EStatueBossAction : uint8
{
	Dormant,
	Idle,
	Walk,
	AxeSlam,
	Stomp UMETA(Hidden),
	FireStomp UMETA(Hidden),
	Defeated,
	Awakening,
	Enraging,
	Staggered,
	Cleave,
	LeapingSlam
};

USTRUCT()
struct FStatueBossActionState
{
	GENERATED_BODY()

	UPROPERTY() EStatueBossAction Action = EStatueBossAction::Dormant;
	UPROPERTY() double StartedAt = 0.0;
	UPROPERTY() float HitAfter = 0.f;
	UPROPERTY() float PlayRate = 1.f;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() float Radius = 0.f;
	UPROPERTY() float HalfWidth = 0.f;
	UPROPERTY() FVector_NetQuantize GroundCenter = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal GroundNormal = FVector::UpVector;
	UPROPERTY() FVector_NetQuantizeNormal Heading = FVector::ForwardVector;
	UPROPERTY() uint16 Revision = 0;
	UPROPERTY() bool bReleased = false;
};

UCLASS()
class BOLDERSDEMO_API AStatueBossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	AStatueBossCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Statue Boss")
	void ActivateBoss();
	void StopForEncounterEnd();
	UFUNCTION(BlueprintPure, Category="Statue Boss")
	bool IsAwakened() const { return ActionState.Action != EStatueBossAction::Dormant; }
	UFUNCTION(BlueprintPure, Category="Statue Boss")
	bool IsEnraged() const { return bEnraged; }
	UFUNCTION(BlueprintPure, Category="Statue Boss")
	EStatueBossAction GetCurrentAction() const { return ActionState.Action; }
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") FVector GetWarningCenter() const { return ActionState.GroundCenter; }
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") float GetWarningRadius() const { return ActionState.Radius; }
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") float GetWarningHalfWidth() const { return ActionState.HalfWidth; }
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") double GetWarningEndsAt() const { return ActionState.StartedAt + ActionState.HitAfter; }
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") bool IsPointInWarning(const FVector& WorldPoint) const;
	UFUNCTION(BlueprintPure, Category="Statue Boss|Telegraph") bool IsAttackWarningAt(const FVector& GroundPosition) const;
	UFUNCTION(BlueprintPure, Category="Statue Boss") bool IsRecovering() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual float GetCombatImpulseScale() const override { return .18f; }

	UPROPERTY(EditAnywhere, Category="Statue Boss|Movement", meta=(ClampMin="800", Units="cm"))
	float ChaseRadius = 14500.f;

	UPROPERTY(EditAnywhere, Category="Statue Boss|Movement", meta=(ClampMin="800", Units="cm"))
	float EngagementRadius = 17500.f;

private:
	void HandleBossHealthChanged(const FOnAttributeChangeData& Change);
	void UpdateAuthority(double Now);
	void DecideAction(double Now);
	ABaseCharacter* FindOpponent() const;
	void StopPursuit();
	void BeginAttack(EStatueBossAction Attack, const ABaseCharacter& Opponent, double Now);
	bool TryBeginLeap(const ABaseCharacter& Opponent, double Now);
	bool AdvanceLeap(double Elapsed);
	void BeginTransition(EStatueBossAction Next, double Now);
	void ResolveAttack();
	void SetAction(FStatueBossActionState NewAction);
	UAnimSequence* GetActionAnimation(EStatueBossAction Action) const;
	float GetAnimationSampleTime(double Elapsed) const;
	double GetCombatTime() const;
	UFUNCTION() void OnRep_ActionState();
	UFUNCTION() void OnRep_Enraged();
	void UpdatePresentation(double Now, float DeltaSeconds);
	void UpdateWalk(float DeltaSeconds);
	void PlayTransitionPresentation(double Elapsed);
	void PlayWorldCue(USoundBase* Sound, const FVector& Origin, float Volume = 1.f) const;
	void PlayImpactPresentation();

	UPROPERTY(VisibleAnywhere, Category="Statue Boss|Presentation") TObjectPtr<UStaticMeshComponent> Axe;
	UPROPERTY(VisibleAnywhere, Category="Statue Boss|Selection") TObjectPtr<UCapsuleComponent> UpperBodySelection;
	UPROPERTY(VisibleAnywhere, Category="Statue Boss|Presentation") TObjectPtr<UDecalComponent> WarningOutline;
	UPROPERTY(VisibleAnywhere, Category="Statue Boss|Presentation") TObjectPtr<UPointLightComponent> FurnaceGlow;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> AxeSlamAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> LeapAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> CleaveAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> AwakeningAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> EnrageAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> StaggerAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UAnimSequence> DefeatAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UMaterialInterface> StoneImpactMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UMaterialInterface> FireImpactMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UMaterialInterface> FireImpactRing;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UNiagaraSystem> StoneImpactParticles;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Presentation") TObjectPtr<UNiagaraSystem> FireImpactParticles;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> FootfallSound;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> AxeImpactSound;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> StompImpactSound;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> AwakeningSound;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> EnrageSound;
	UPROPERTY(EditDefaultsOnly, Category="Statue Boss|Audio") TObjectPtr<USoundBase> DefeatSound;
	UPROPERTY() TObjectPtr<USoundAttenuation> CueAttenuation;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> OutlineMaterial;
	UPROPERTY(ReplicatedUsing=OnRep_ActionState) FStatueBossActionState ActionState;
	UPROPERTY(ReplicatedUsing=OnRep_Enraged) bool bEnraged = false;

	FVector HomeLocation = FVector::ZeroVector;
	FVector LeapStart = FVector::ZeroVector;
	FVector LeapEnd = FVector::ZeroVector;
	TWeakObjectPtr<ABaseCharacter> PursuitTarget;
	FDelegateHandle HealthChangedHandle;
	double NextDecisionAt = 0.0;
	double NextPursuitAt = 0.0;
	double NextAttackAt = 0.0;
	double LastHitFlinchAt = -10.0;
	float PendingAttackDamage = 0.f;
	float WalkTime = 0.f;
	float WalkSpeed = 0.f;
	float AttackStartYaw = 0.f;
	int32 AttacksStarted = 0;
	uint16 PresentedRevision = MAX_uint16;
	bool bEnrageQueued = false;
	bool bEncounterStopped = false;
	bool bImpactPresented = false;
	bool bTransitionPresented = false;
	bool bDefeatAxePresented = false;
};
