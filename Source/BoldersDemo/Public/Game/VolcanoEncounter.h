#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "VolcanoEncounter.generated.h"

class ABaseCharacter;
class AStaticMeshActor;
class ADecalActor;
class AStatueBossCharacter;
class ATargetPoint;
class UAnimMontage;
class UAnimSequence;
class UMaterialInterface;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

UENUM(BlueprintType)
enum class EVolcanoEncounterStage : uint8
{
	AwaitingPlayers, Guardians, Statue, Victory, Defeat
};

UENUM(BlueprintType)
enum class EVolcanoGuardianRole : uint8
{
	None, Scrapper, Sporecaster, Bulwark
};

UENUM(BlueprintType)
enum class EVolcanoGuardianPhase : uint8
{
	Idle, Emerging, Windup, InFlight, Recovery, Interrupted, Defeated
};

USTRUCT()
struct FVolcanoGuardianCue
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<ABaseCharacter> Character;
	UPROPERTY() EVolcanoGuardianRole Role = EVolcanoGuardianRole::None;
	UPROPERTY() EVolcanoGuardianPhase Phase = EVolcanoGuardianPhase::Idle;
	UPROPERTY() FVector_NetQuantize10 Aim = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 Release = FVector::ZeroVector;
	UPROPERTY() double StartedAt = 0.0;
};

USTRUCT(BlueprintType)
struct FVolcanoPlayerResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Volcano") int32 Slot = 0;
	UPROPERTY(BlueprintReadOnly, Category="Volcano") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Volcano") float MaxHealth = 0.f;
};

USTRUCT(BlueprintType)
struct FVolcanoRunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Volcano") float Seconds = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Volcano") TArray<FVolcanoPlayerResult> Party;
};

UCLASS()
class BOLDERSDEMO_API AVolcanoEncounter : public AActor
{
	GENERATED_BODY()

public:
	AVolcanoEncounter();
	static AVolcanoEncounter* Find(UWorld* World);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintPure, Category="Volcano") EVolcanoEncounterStage GetStage() const { return Stage; }
	bool IsFinished() const { return Stage == EVolcanoEncounterStage::Victory || Stage == EVolcanoEncounterStage::Defeat; }
	const FVolcanoRunResult& GetRunResult() const { return RunResult; }
	UFUNCTION(BlueprintPure, Category="Volcano") int32 GetRemainingGuardians() const { return RemainingGuardians; }
	UFUNCTION(BlueprintPure, Category="Volcano") int32 GetWave() const { return Wave; }
	UFUNCTION(BlueprintPure, Category="Volcano") AStatueBossCharacter* GetBoss() const;
	UFUNCTION(BlueprintPure, Category="Volcano") AActor* GetTravelObjective() const;
	UFUNCTION(BlueprintPure, Category="Volcano") EVolcanoGuardianRole GetGuardianRole(const ABaseCharacter* Guardian) const;
	UFUNCTION(BlueprintPure, Category="Volcano") EVolcanoGuardianPhase GetGuardianPhase(const ABaseCharacter* Guardian) const;
	bool IsGuardianWarningAt(const FVector& GroundPosition) const;
	bool RequestRestart();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FGuardianState
	{
		TWeakObjectPtr<ABaseCharacter> Target;
		TWeakObjectPtr<const ABaseCharacter> ImpactTarget;
		float HealthAtWindup = 0.f;
		double NextMoveAt = 0.0;
		double NextAttackAt = 0.0;
	};
	struct FGuardianVisual
	{
		TWeakObjectPtr<ADecalActor> Warning;
		TWeakObjectPtr<AStaticMeshActor> Spore;
		TWeakObjectPtr<UAnimMontage> Montage;
		double CueStartedAt = -1.0;
		bool bAttackPoseStarted = false;
		bool bStyleApplied = false;
		void ClearMeshes();
	};

	const FVolcanoGuardianCue* FindGuardianCue(const ABaseCharacter* Guardian) const;
	void UpdateEncounter(double Now);
	void StartWave(int32 WaveNumber, double Now);
	void SetStage(EVolcanoEncounterStage NewStage, double Now, const TArray<ABaseCharacter*>& Roster);
	UFUNCTION() void OnRep_Stage(EVolcanoEncounterStage PreviousStage);
	void SetGuardianPhase(FVolcanoGuardianCue& Cue, EVolcanoGuardianPhase Phase, double Now);
	void UpdateGuardian(FGuardianState& Guard, FVolcanoGuardianCue& Cue, const TArray<ABaseCharacter*>& Party, double Now);
	void BeginGuardianAttack(FGuardianState& Guard, FVolcanoGuardianCue& Cue, const ABaseCharacter& Target,
		const TArray<ABaseCharacter*>& Party, double Now);
	double GetGuardianWindupTime(const FVolcanoGuardianCue& Cue, const FVector& Aim, const ABaseCharacter& Target,
		const TArray<ABaseCharacter*>& Party, double Now) const;
	void ResolveGuardianAttack(FVolcanoGuardianCue& Cue, const TArray<ABaseCharacter*>& Party, double Now);
	bool CheckpointParty(const TArray<ABaseCharacter*>& Roster, const TArray<ABaseCharacter*>& Party) const;
	bool FindRevivalPoint(const ABaseCharacter& Member, const TArray<ABaseCharacter*>& Party, FVector& Center) const;
	void UpdateGuardianVisuals(double Now);
	void ApplyGuardianVisual(const FVolcanoGuardianCue& Cue, FGuardianVisual& Visual, float Age);
	bool ApplyGuardianStyle(const FVolcanoGuardianCue& Cue) const;
	void PlayGuardianCueAudio(const FVolcanoGuardianCue& Cue, float Age) const;
	void UpdateGuardianAttackPose(const FVolcanoGuardianCue& Cue, FGuardianVisual& Visual, float Age) const;
	UAnimSequence* GetGuardianAttackAnimation(EVolcanoGuardianRole Role) const;
	AStaticMeshActor* SpawnGuardianMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Position,
		const FVector& Scale, FName Tag, float Lifespan) const;

	UPROPERTY(EditInstanceOnly, Category="Encounter") TObjectPtr<AStatueBossCharacter> Boss;
	UPROPERTY(EditInstanceOnly, Category="Encounter") TSubclassOf<ABaseCharacter> GuardianClass;
	UPROPERTY(EditInstanceOnly, Category="Encounter") TArray<TObjectPtr<ATargetPoint>> FirstWaveSpawns;
	UPROPERTY(EditInstanceOnly, Category="Encounter") TArray<TObjectPtr<ATargetPoint>> SecondWaveSpawns;
	UPROPERTY(EditInstanceOnly, Category="Encounter") float ArenaFallZ = 1420.f;
	UPROPERTY(EditInstanceOnly, Category="Encounter") bool bProgressByRegion = false;
	UPROPERTY() TObjectPtr<UAnimSequence> GuardianWakeAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> GuardianBraceAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> ScrapperAttackAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> SporecasterAttackAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> BulwarkAttackAnimation;
	UPROPERTY() TObjectPtr<UMaterialInterface> TelegraphDecal;
	UPROPERTY() TObjectPtr<UStaticMesh> SporeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> SporeMaterial;
	UPROPERTY() TObjectPtr<UNiagaraSystem> ScrapperImpact;
	UPROPERTY() TObjectPtr<UNiagaraSystem> SporeImpact;
	UPROPERTY() TObjectPtr<UNiagaraSystem> BulwarkImpact;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> GuardianSkins;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> GuardianWarnings;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> GuardianContacts;
	UPROPERTY() TObjectPtr<USoundAttenuation> GuardianAttenuation;
	UPROPERTY(EditInstanceOnly, Category="Encounter") TObjectPtr<USoundBase> PartyDefeatCue;
	UPROPERTY(ReplicatedUsing=OnRep_Stage) EVolcanoEncounterStage Stage = EVolcanoEncounterStage::AwaitingPlayers;
	UPROPERTY(Replicated) int32 Wave = 0;
	UPROPERTY(Replicated) int32 RemainingGuardians = 0;
	UPROPERTY(Replicated) TArray<FVolcanoGuardianCue> GuardianCues;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Volcano", meta=(AllowPrivateAccess="true"))
	FVolcanoRunResult RunResult;
	TArray<FGuardianState> Guardians;
	TMap<TWeakObjectPtr<ABaseCharacter>, FGuardianVisual> GuardianVisuals;
	double AdvanceAt = 0.0;
	double NextEncounterTickAt = 0.0;
	double RunStartedAt = 0.0;
	bool bRestarting = false;
};
