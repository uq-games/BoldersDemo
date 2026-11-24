#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Data/AttackLoadout.h"
#include "GameFramework/Actor.h"
#include "VolcanoHazard.generated.h"

class ABaseCharacter;
class UAbilitySystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProceduralMeshComponent;
class USoundAttenuation;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EVolcanoHazardKind : uint8
{
	Eruption, Puddle, ExplosiveBarrel, LightningOrb
};

UENUM(BlueprintType)
enum class EVolcanoHazardState : uint8
{
	Ready, Warning, Active, Cooling, Frozen, Steaming, Spent
};

USTRUCT()
struct FVolcanoHazardPhase
{
	GENERATED_BODY()

	UPROPERTY() EVolcanoHazardState State = EVolcanoHazardState::Ready;
	UPROPERTY() float StartedAt = 0.f;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() uint16 Revision = 0;
	UPROPERTY() bool bEncounterEnded = false;
};

UCLASS()
class BOLDERSDEMO_API AVolcanoHazard : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AVolcanoHazard();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="Volcano Hazard")
	EVolcanoHazardKind Kind = EVolcanoHazardKind::Eruption;
	// The actor origin is the center of the field at ground level. Zero uses the kind's default radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Configuration, Category="Volcano Hazard",
		meta=(ClampMin="0", ClampMax="1000", Units="cm"))
	float Radius = 0.f;
	// Eruptions wait this many additional seconds before their first warning.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volcano Hazard", meta=(ClampMin="0", Units="s"))
	float CycleOffset = 0.f;

	UFUNCTION(BlueprintPure, Category="Volcano Hazard") EVolcanoHazardKind GetHazardKind() const { return Kind; }
	UFUNCTION(BlueprintPure, Category="Volcano Hazard") EVolcanoHazardState GetHazardState() const { return Phase.State; }
	UFUNCTION(BlueprintPure, Category="Volcano Hazard") bool CanReceiveAttack() const;
	bool IsEncounterActive() const { return !Phase.bEncounterEnded; }
	bool CanReactTo(EDamageElement Element) const;
	UFUNCTION(BlueprintPure, Category="Volcano Hazard") FVector GetAttackPoint() const;
	UFUNCTION(BlueprintPure, Category="Volcano Hazard") float GetAttackRadius() const;
	UFUNCTION(BlueprintPure, Category="Volcano Hazard") float GetEffectRadius() const;
	bool HasGroundWarning() const;
	bool ThreatensGround(const FVector& Position) const;
	bool IntersectsAreaImpact(const FVector& Center, float AreaRadius) const;
	// Closest water/ice contact on the actual basin footprint.
	EDamageElement GetBladeDipContact(const FVector& Feet, FVector& Contact) const;
	void SetLocalFocus(bool bFocused);
	void DeactivateForEncounterEnd();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Volcano Hazard")
	bool ReceiveAttack(ABaseCharacter* Source, EDamageElement Element);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void OnRep_Configuration();
	UFUNCTION() void OnRep_Phase();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastReaction(EElementalReaction Reaction, FVector_NetQuantize Location, FVector_NetQuantize End);

	void ChangePhase(EVolcanoHazardState State, float Seconds);
	void AdvancePhase();
	void Pulse();
	void AffectCharacters(EDamageElement Element, float Damage);
	bool ApplyToPool(EDamageElement Element);
	void TriggerNeighbors();
	bool IsUnobstructed(const FVector& WorldLocation) const;
	float GetPhaseAge() const;

	void LoadPresentationAssets();
	void ConfigurePresentation();
	void UpdatePresentation();
	void UpdatePhaseEffect();
	bool PlayConductLink(const FVector& Start, const FVector& End) const;
	void BuildCoolingBasin();
	void BuildGroundMark();
	void PlayPhaseSound(float PhaseAge) const;

	UPROPERTY(VisibleAnywhere, Category="Volcano Hazard")
	TObjectPtr<USphereComponent> AttackTarget;
	UPROPERTY(VisibleAnywhere, Category="Volcano Hazard")
	TObjectPtr<USphereComponent> OrbPedestalTarget;
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY(ReplicatedUsing=OnRep_Phase)
	FVolcanoHazardPhase Phase;

	FTimerHandle PhaseTimer;
	FTimerHandle PulseTimer;

	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UProceduralMeshComponent> GroundMark;
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UProceduralMeshComponent> WaterSurface;
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(VisibleAnywhere, Category="Presentation") TObjectPtr<UNiagaraComponent> PhaseEffect;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> FixtureMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> FixtureEffects;
	UStaticMesh* GetFixtureMesh(int32 Index) const;
	UNiagaraSystem* GetFixtureEffect(int32 Index) const;
	UPROPERTY() TObjectPtr<UNiagaraSystem> PresentedEffect;
	uint16 PresentedRevision = 0;
	bool bPhasePresentationReady = false;
	UPROPERTY() TObjectPtr<UMaterialInterface> FireCoreMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> OrbMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> PoolMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> IceMaterial;
	UPROPERTY() TObjectPtr<UElementalReactionAppearance> Reactions;
	UPROPERTY() TObjectPtr<USoundAttenuation> SoundAttenuation;
	UPROPERTY() TObjectPtr<USoundBase> EruptionSound;
	UPROPERTY() TObjectPtr<USoundBase> BarrelSound;
	UPROPERTY() TObjectPtr<USoundBase> OrbSound;
	UPROPERTY() TObjectPtr<USoundBase> SteamSound;
	UPROPERTY() TObjectPtr<USoundBase> FreezeSound;
};
