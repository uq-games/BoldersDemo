#include "Game/VolcanoEncounter.h"
#include "Game/VolcanoGuardianProfile.h"

#include "AbilitySystemComponent.h"
#include "AI/Positioning/AttackPositionRules.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Characters/BaseCharacter.h"
#include "Characters/StatueBoss/StatueBossCharacter.h"
#include "Combat/Attacks/AttackResolution.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Combat/Effects/BoldersAttributeSet.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolcanoGuardians, Log, All);

namespace
{
	using GuardianRoleType = EVolcanoGuardianRole;
	using Phase = EVolcanoGuardianPhase;
	using namespace VolcanoGuardians;

	bool IsImpactPending(Phase GuardianPhase)
	{
		return GuardianPhase == Phase::Windup || GuardianPhase == Phase::InFlight;
	}

	bool IsWithinGuardianFootprint(const FVector& Point, const FVector& Aim, float Radius)
	{
		const FVector Offset = Point - Aim;
		return Offset.SizeSquared2D() <= FMath::Square(Radius) && FMath::Abs(Offset.Z) <= GuardianFootprintHeight;
	}

	double GuardianImpactDelay(EVolcanoGuardianRole GuardianRole)
	{
		const float Flight = GuardianRole == GuardianRoleType::Sporecaster ? SporeTravel : 0.f;
		return static_cast<double>(GetProfile(GuardianRole).Windup) + Flight;
	}

	template<typename T>
	T* LoadGuardianAsset(const TCHAR* Path)
	{
		return ConstructorHelpers::FObjectFinderOptional<T>(Path).Get();
	}

	FVector GuardianFeet(const ABaseCharacter& Guardian)
	{
		return Guardian.GetActorLocation()
			- FVector::UpVector * Guardian.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	FVector ProjectGuardianAim(const ABaseCharacter& Guardian, const FVector& Estimate)
	{
		FHitResult Ground;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardianAimGround), false, &Guardian);
		const FVector Vertical(0, 0, 85.f);
		return Guardian.GetWorld()->LineTraceSingleByObjectType(Ground, Estimate + Vertical, Estimate - Vertical,
			FCollisionObjectQueryParams(ECC_WorldStatic), Params) && Ground.ImpactNormal.Z > .7f
			? Ground.ImpactPoint : Estimate;
	}

	void StopGuardianMovement(ABaseCharacter& Guardian)
	{
		if (auto* AI = Cast<AAIController>(Guardian.GetController()); AI && AI->GetMoveStatus() != EPathFollowingStatus::Idle)
			AI->StopMovement();
		Guardian.GetCharacterMovement()->StopMovementImmediately();
	}
}

AVolcanoEncounter::AVolcanoEncounter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Wake(TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_IdlePlantToBattleSmile_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Brace(TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_IdleBattleSmile_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Scrapper(TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_Attack02Smile_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Sporecaster(TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_Attack01Smile_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Bulwark(TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_Attack03Smile_ANIM"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Decal(CombatGuideStyle::DangerMaterialPath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Spore(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SporeGlow(TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/M_Concept_FireballHotCore"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SwipeFX(TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/NS_Impact_Gold"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SporeFX(TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/NS_Impact_Ember"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlamFX(TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/NS_Impact_Steel"));
	GuardianWakeAnimation = Wake.Object;
	GuardianBraceAnimation = Brace.Object;
	ScrapperAttackAnimation = Scrapper.Object;
	SporecasterAttackAnimation = Sporecaster.Object;
	BulwarkAttackAnimation = Bulwark.Object;
	TelegraphDecal = Decal.Object;
	SporeMesh = Spore.Object;
	SporeMaterial = SporeGlow.Object;
	ScrapperImpact = SwipeFX.Object;
	SporeImpact = SporeFX.Object;
	BulwarkImpact = SlamFX.Object;
	GuardianSkins = {
		LoadGuardianAsset<UMaterialInterface>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Materials/MI_GuardianScrapper")),
		LoadGuardianAsset<UMaterialInterface>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Materials/MI_GuardianSporecaster")),
		LoadGuardianAsset<UMaterialInterface>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Materials/MI_GuardianBulwark"))
	};
	GuardianWarnings = {
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianScrapperWarn")),
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianSporeWarn")),
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianBulwarkWarn"))
	};
	GuardianContacts = {
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianScrapperHit")),
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianSporeLaunch")),
		LoadGuardianAsset<USoundBase>(TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio/SW_GuardianBulwarkHit"))
	};
	GuardianAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("GuardianSoundAttenuation"));
	GuardianAttenuation->Attenuation.bSpatialize = true;
	GuardianAttenuation->Attenuation.bAttenuate = true;
	GuardianAttenuation->Attenuation.AttenuationShapeExtents = FVector(820.f, 0.f, 0.f);
	GuardianAttenuation->Attenuation.FalloffDistance = 2400.f;
}

AVolcanoEncounter* AVolcanoEncounter::Find(UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<AVolcanoEncounter> It(World); It; ++It) return *It;
	return nullptr;
}

AStatueBossCharacter* AVolcanoEncounter::GetBoss() const { return Boss; }

AActor* AVolcanoEncounter::GetTravelObjective() const
{
	if (!bProgressByRegion || RemainingGuardians > 0) return nullptr;
	if (Stage == EVolcanoEncounterStage::AwaitingPlayers)
		return FirstWaveSpawns.IsEmpty() ? nullptr : FirstWaveSpawns[0].Get();
	if (Stage == EVolcanoEncounterStage::Guardians)
	{
		if (Wave == 1) return SecondWaveSpawns.IsEmpty() ? nullptr : SecondWaveSpawns[0].Get();
		return Boss;
	}
	return nullptr;
}

const FVolcanoGuardianCue* AVolcanoEncounter::FindGuardianCue(const ABaseCharacter* Guardian) const
{
	return IsValid(Guardian) ? GuardianCues.FindByPredicate([Guardian](const FVolcanoGuardianCue& Cue)
	{
		return Cue.Character.Get() == Guardian;
	}) : nullptr;
}

EVolcanoGuardianRole AVolcanoEncounter::GetGuardianRole(const ABaseCharacter* Guardian) const
{
	const auto* Cue = FindGuardianCue(Guardian);
	return Cue ? Cue->Role : GuardianRoleType::None;
}

EVolcanoGuardianPhase AVolcanoEncounter::GetGuardianPhase(const ABaseCharacter* Guardian) const
{
	const auto* Cue = FindGuardianCue(Guardian);
	return Cue ? Cue->Phase : Phase::Idle;
}

bool AVolcanoEncounter::IsGuardianWarningAt(const FVector& GroundPosition) const
{
	return GuardianCues.ContainsByPredicate([&GroundPosition](const FVolcanoGuardianCue& Cue)
	{
		return IsImpactPending(Cue.Phase) && IsValid(Cue.Character) && Cue.Character->IsAlive()
			&& IsWithinGuardianFootprint(GroundPosition, Cue.Aim, GetProfile(Cue.Role).Radius);
	});
}

void AVolcanoEncounter::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) SetActorTickInterval(.04f);
#if WITH_EDITOR
	else if (TelegraphDecal && FApp::CanEverRender())
	{
		// Finish the decal shader during level startup so the first warning has its real silhouette.
		TelegraphDecal->EnsureIsComplete();
	}
#endif
}

void AVolcanoEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVolcanoEncounter, Stage);
	DOREPLIFETIME(AVolcanoEncounter, Wave);
	DOREPLIFETIME(AVolcanoEncounter, RemainingGuardians);
	DOREPLIFETIME(AVolcanoEncounter, GuardianCues);
	DOREPLIFETIME(AVolcanoEncounter, RunResult);
}

void AVolcanoEncounter::SetStage(EVolcanoEncounterStage NewStage, double Now, const TArray<ABaseCharacter*>& Roster)
{
	if (NewStage == Stage) return;
	const EVolcanoEncounterStage PreviousStage = Stage;
	Stage = NewStage;
	if (IsFinished())
	{
		RunResult.Seconds = static_cast<float>(FMath::Max(0., Now - RunStartedAt));
		for (const auto* Member : Roster)
			RunResult.Party.Add({Member->PlayerSlot, Member->GetHealth(), Member->GetMaxHealth()});
		RunResult.Party.Sort([](const auto& A, const auto& B) { return A.Slot < B.Slot; });
		if (Stage == EVolcanoEncounterStage::Defeat && IsValid(Boss)) Boss->StopForEncounterEnd();
		for (TActorIterator<AVolcanoHazard> It(GetWorld()); It; ++It) It->DeactivateForEncounterEnd();
	}
	OnRep_Stage(PreviousStage);
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("VolcanoEncounter: stage=%s wave=%d"), *UEnum::GetValueAsString(Stage), Wave);
}

void AVolcanoEncounter::OnRep_Stage(EVolcanoEncounterStage PreviousStage)
{
	if (Stage == EVolcanoEncounterStage::Defeat && PartyDefeatCue && GetNetMode() != NM_DedicatedServer
		&& (PreviousStage == EVolcanoEncounterStage::Guardians || PreviousStage == EVolcanoEncounterStage::Statue))
	{
		UGameplayStatics::PlaySound2D(this, PartyDefeatCue, .86f);
	}
}

void AVolcanoEncounter::StartWave(int32 WaveNumber, double Now)
{
	Guardians.Reset();
	GuardianCues.Reset();
	Wave = WaveNumber;
	const auto& Spawns = Wave == 1 ? FirstWaveSpawns : SecondWaveSpawns;
	constexpr GuardianRoleType GuardiansInOrder[] = {GuardianRoleType::Scrapper, GuardianRoleType::Sporecaster, GuardianRoleType::Bulwark};
	if (Wave == 1) RunStartedAt = Now;
	for (int32 Index = 0; Index < Spawns.Num(); ++Index)
	{
		ATargetPoint* Spawn = Spawns[Index];
		if (!IsValid(Spawn) || !GuardianClass) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		auto* Guardian = GetWorld()->SpawnActor<ABaseCharacter>(GuardianClass, Spawn->GetActorTransform(), Params);
		if (!Guardian) continue;
		const GuardianRoleType GuardianRole = GuardiansInOrder[FMath::Min(Index, static_cast<int32>(UE_ARRAY_COUNT(GuardiansInOrder)) - 1)];
		const FProfile& Profile = GetProfile(GuardianRole);
		Guardian->GetCharacterMovement()->MaxWalkSpeed = Profile.WalkSpeed;
		const float Health = Profile.Health + (Wave == 2 && GuardianRole != GuardianRoleType::Bulwark ? 18.f : 0.f);
		auto* ASC = Guardian->GetAbilitySystemComponent();
		ASC->SetNumericAttributeBase(UBoldersAttributeSet::GetMaxHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), Health);
		auto& State = Guardians.AddDefaulted_GetRef();
		State.NextAttackAt = Now + SpawnTime + .5 + .45 * Index;
		auto& Cue = GuardianCues.AddDefaulted_GetRef();
		Cue.Character = Guardian;
		Cue.Role = GuardianRole;
		SetGuardianPhase(Cue, Phase::Emerging, Now);
	}
	RemainingGuardians = Guardians.Num();
	AdvanceAt = 0.0;
	SetStage(EVolcanoEncounterStage::Guardians, Now, {});
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("VolcanoEncounter: wave=%d guardians=%d"), Wave, RemainingGuardians);
}

bool AVolcanoEncounter::FindRevivalPoint(const ABaseCharacter& Member, const TArray<ABaseCharacter*>& Party,
	FVector& Center) const
{
	const auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return false;
	const auto* Capsule = Member.GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight() - 3.f);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CheckpointRevival), false, &Member);
	const AttackPositionRules::FApproachWarnings Warnings(Member);
	TArray<FVector> Anchors;
	for (const auto* Survivor : Party) Anchors.Add(GuardianFeet(*Survivor));
	Anchors.Sort([&Member](const FVector& A, const FVector& B)
	{
		return FVector::DistSquared2D(A, Member.GetActorLocation()) < FVector::DistSquared2D(B, Member.GetActorLocation());
	});
	if (const auto* Objective = GetTravelObjective()) Anchors.Add(Objective->GetActorLocation());
	for (const FVector& Anchor : Anchors)
	{
		for (const float Radius : {140.f, 240.f, 350.f, 470.f})
		{
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const double Angle = Step * UE_DOUBLE_PI / 4.0;
				const FVector Desired = Anchor + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius;
				FNavLocation Point;
				if (!Nav->ProjectPointToNavigation(Desired, Point, FVector(55, 55, 155))
					|| Point.Location.Z < ArenaFallZ || !Warnings.IsPointSafe(Point.Location)) continue;
				const FVector Spot = Point.Location + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
				if (GetWorld()->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Pawn, Shape, Query)) continue;
				Center = Spot;
				return true;
			}
		}
	}
	return false;
}

bool AVolcanoEncounter::CheckpointParty(const TArray<ABaseCharacter*>& Roster, const TArray<ABaseCharacter*>& Party) const
{
	TArray<TPair<ABaseCharacter*, FVector>, TInlineAllocator<2>> Revivals;
	for (auto* Member : Roster)
	{
		if (Member->IsAlive()) continue;
		FVector Point;
		if (!FindRevivalPoint(*Member, Party, Point)) return false;
		Revivals.Emplace(Member, Point);
	}
	for (ABaseCharacter* Member : Party)
	{
		const float Restored = FMath::Min(Member->GetMaxHealth(), Member->GetHealth() + Member->GetMaxHealth() * .30f);
		Member->GetAbilitySystemComponent()->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), Restored);
	}
	for (const auto& Revival : Revivals) Revival.Key->ReviveAtCheckpoint(Revival.Value);
	return true;
}

void AVolcanoEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (HasAuthority() && Now >= NextEncounterTickAt)
	{
		// A consistent AI cadence while projectile presentation remains smooth at render rate.
		NextEncounterTickAt = Now + .04;
		UpdateEncounter(Now);
	}
	if (GetNetMode() != NM_DedicatedServer) UpdateGuardianVisuals(Now);
}

void AVolcanoEncounter::UpdateEncounter(double Now)
{
	if (IsFinished()) return;

	TArray<ABaseCharacter*> Party;
	TArray<ABaseCharacter*> Roster;
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		if (It->PlayerSlot > 0) Roster.Add(*It);
		if (!It->IsAlive() || (It->PlayerSlot <= 0 && It->GetFaction() != ECharacterFaction::Enemy)) continue;
		if (It->GetActorLocation().Z < ArenaFallZ)
			It->GetAbilitySystemComponent()->SetNumericAttributeBase(UBoldersAttributeSet::GetHealthAttribute(), 0.f);
		else if (It->PlayerSlot > 0) Party.Add(*It);
	}

	if (Stage == EVolcanoEncounterStage::AwaitingPlayers)
	{
		const auto* Objective = GetTravelObjective();
		const bool bArrived = !Party.IsEmpty() && (!Objective || Party.ContainsByPredicate([Objective](const ABaseCharacter* Member)
		{
			return FVector::Dist2D(Member->GetActorLocation(), Objective->GetActorLocation()) <= 960.f;
		}));
		if (!bArrived) AdvanceAt = 0.0;
		else if (AdvanceAt == 0.0) AdvanceAt = Now + (bProgressByRegion ? 1.2 : 3.0);
		else if (Now >= AdvanceAt) StartWave(1, Now);
		return;
	}
	if (Party.IsEmpty())
	{
		for (FVolcanoGuardianCue& Cue : GuardianCues)
		{
			if (!IsValid(Cue.Character) || !Cue.Character->IsAlive()) continue;
			StopGuardianMovement(*Cue.Character);
			SetGuardianPhase(Cue, Phase::Idle, Now);
		}
		SetStage(EVolcanoEncounterStage::Defeat, Now, Roster);
		return;
	}
	if (IsValid(Boss) && Stage == EVolcanoEncounterStage::Statue && !Boss->IsAlive())
	{
		SetStage(EVolcanoEncounterStage::Victory, Now, Roster);
		return;
	}
	if (Stage != EVolcanoEncounterStage::Guardians) return;

	int32 Living = 0;
	for (int32 Index = 0; Index < Guardians.Num(); ++Index)
	{
		FVolcanoGuardianCue& Cue = GuardianCues[Index];
		auto* Character = Cue.Character.Get();
		if (!IsValid(Character) || Cue.Phase == Phase::Defeated) continue;
		if (!Character->IsAlive())
		{
			SetGuardianPhase(Cue, Phase::Defeated, Now);
			Character->SetActorEnableCollision(false);
			if (AAIController* AI = Cast<AAIController>(Character->GetController())) AI->Destroy();
			Character->SetLifeSpan(2.2f);
			continue;
		}
		++Living;
		UpdateGuardian(Guardians[Index], Cue, Party, Now);
	}
	if (Living != RemainingGuardians)
	{
		RemainingGuardians = Living;
		ForceNetUpdate();
	}
	if (Living > 0) return;
	if (AdvanceAt == 0.0) AdvanceAt = Now + 2.3;
	if (Now < AdvanceAt) return;
	if (const AActor* Objective = GetTravelObjective())
	{
		const float ArrivalRadius = Wave == 1 ? 1100.f : 975.f;
		if (!Party.ContainsByPredicate([Objective, ArrivalRadius](const ABaseCharacter* Member)
		{
			const FVector Delta = Member->GetActorLocation() - Objective->GetActorLocation();
			return Delta.SizeSquared2D() <= FMath::Square(ArrivalRadius) && FMath::Abs(Delta.Z) < 265.f;
		})) return;
	}
	if ((Wave != 1 && !IsValid(Boss)) || !CheckpointParty(Roster, Party)) return;
	if (Wave == 1) StartWave(2, Now);
	else if (IsValid(Boss))
	{
		Boss->ActivateBoss();
		SetStage(EVolcanoEncounterStage::Statue, Now, Roster);
	}
}

void AVolcanoEncounter::SetGuardianPhase(FVolcanoGuardianCue& Cue, EVolcanoGuardianPhase NewPhase, double Now)
{
	if (Cue.Phase == NewPhase) return;
	Cue.Phase = NewPhase;
	Cue.StartedAt = Now;
	if (Cue.Role == GuardianRoleType::Bulwark)
	{
		const bool bExposed = NewPhase == Phase::Recovery || NewPhase == Phase::Interrupted;
		Cue.Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UBoldersAttributeSet::GetPhysicalResistanceAttribute(),
			bExposed ? BulwarkExposedArmor : BulwarkArmor);
	}
	ForceNetUpdate();
}

void AVolcanoEncounter::UpdateGuardian(FGuardianState& Guard, FVolcanoGuardianCue& Cue,
	const TArray<ABaseCharacter*>& Party, double Now)
{
	ABaseCharacter& Character = *Cue.Character;
	const FProfile& Profile = GetProfile(Cue.Role);
	const double Age = Now - Cue.StartedAt;
	const bool bGrounded = Character.GetCharacterMovement()->IsMovingOnGround();
	switch (Cue.Phase)
	{
	case Phase::Emerging:
		if (Age >= SpawnTime) SetGuardianPhase(Cue, Phase::Idle, Now);
		return;
	case Phase::Windup:
		// The shared hit-reaction slot takes over on damage. Cancel the pending strike as well.
		if (!bGrounded || Character.IsFrozen() || Character.GetHealth() < Guard.HealthAtWindup - UE_KINDA_SMALL_NUMBER)
		{
			SetGuardianPhase(Cue, Phase::Interrupted, Now);
			UE_LOG(LogVolcanoGuardians, Log, TEXT("%s windup interrupted"), *Character.GetName());
			return;
		}
		if (Age < Profile.Windup) return;
		if (Cue.Role == GuardianRoleType::Sporecaster)
			SetGuardianPhase(Cue, Phase::InFlight, Cue.StartedAt + Profile.Windup);
		else
			ResolveGuardianAttack(Cue, Party, Now);
		return;
	case Phase::InFlight:
		if (Age >= SporeTravel) ResolveGuardianAttack(Cue, Party, Now);
		return;
	case Phase::Recovery:
		if (Age >= Profile.Recovery) SetGuardianPhase(Cue, Phase::Idle, Now);
		return;
	case Phase::Interrupted:
		if (Age >= InterruptTime && !Character.IsFrozen()) SetGuardianPhase(Cue, Phase::Idle, Now);
		return;
	case Phase::Defeated:
		return;
	case Phase::Idle:
		break;
	}
	if (!bGrounded || Character.IsFrozen()) return;

	ABaseCharacter* Nearest = nullptr;
	ABaseCharacter* InReach = nullptr;
	double NearestDistance = MAX_dbl;
	double AttackDistance = FMath::Square(static_cast<double>(Profile.Reach));
	for (ABaseCharacter* Member : Party)
	{
		if (!Member->IsAlive()) continue;
		const double Distance = FVector::DistSquared2D(Character.GetActorLocation(), Member->GetActorLocation());
		if (Distance < NearestDistance)
		{
			Nearest = Member;
			NearestDistance = Distance;
		}
		if (Distance < AttackDistance && FMath::Abs(GuardianFeet(Character).Z - GuardianFeet(*Member).Z) < 115.f
			&& AttackResolution::HasClearPath(Character, *Member, Character.GetActorLocation()))
		{
			InReach = Member;
			AttackDistance = Distance;
		}
	}
	if (InReach)
	{
		StopGuardianMovement(Character);
		if (Now >= Guard.NextAttackAt)
			BeginGuardianAttack(Guard, Cue, *InReach, Party, Now);
		return;
	}
	if (auto* AI = Cast<AAIController>(Character.GetController()); AI && Nearest && Now >= Guard.NextMoveAt
		&& (Guard.Target.Get() != Nearest || AI->GetMoveStatus() == EPathFollowingStatus::Idle))
	{
		Guard.NextMoveAt = Now + .42;
		Guard.Target = Nearest;
		const float ChaseRadius = Cue.Role == GuardianRoleType::Sporecaster ? Profile.Reach * .68f : 64.f;
		AI->MoveToActor(Nearest, ChaseRadius, false, true, true, nullptr, false);
	}
}

double AVolcanoEncounter::GetGuardianWindupTime(const FVolcanoGuardianCue& Cue, const FVector& Aim,
	const ABaseCharacter& Target, const TArray<ABaseCharacter*>& Party, double Now) const
{
	const float Radius = GetProfile(Cue.Role).Radius;
	const double OverlappingImpactSpacing = Wave == 1 ? .55 : .48;
	const double ImpactDelay = GuardianImpactDelay(Cue.Role);
	const double ProposedImpactAt = Now + ImpactDelay;
	double EarliestWindup = Now;
	for (int32 Index = 0; Index < GuardianCues.Num(); ++Index)
	{
		const FVolcanoGuardianCue& Other = GuardianCues[Index];
		if (&Other == &Cue || !IsImpactPending(Other.Phase) || !IsValid(Other.Character) || !Other.Character->IsAlive()) continue;
		const double OtherImpactAt = Other.StartedAt + (Other.Phase == Phase::InFlight ? SporeTravel : GuardianImpactDelay(Other.Role));
		if (FMath::Abs(OtherImpactAt - ProposedImpactAt) >= OverlappingImpactSpacing) continue;

		const float OtherRadius = GetProfile(Other.Role).Radius;
		if (FVector::DistSquared2D(Aim, Other.Aim) > FMath::Square(Radius + OtherRadius)
			|| FMath::Abs(Aim.Z - Other.Aim.Z) > 2.f * GuardianFootprintHeight) continue;
		const bool bShareThreat = Guardians[Index].ImpactTarget.Get() == &Target
			|| Party.ContainsByPredicate([&Aim, Radius, &Other, OtherRadius](const ABaseCharacter* Member)
			{
				if (!Member->IsAlive()) return false;
				const FVector Feet = GuardianFeet(*Member);
				return IsWithinGuardianFootprint(Feet, Aim, Radius) && IsWithinGuardianFootprint(Feet, Other.Aim, OtherRadius);
			});
		if (bShareThreat)
			EarliestWindup = FMath::Max(EarliestWindup, OtherImpactAt + OverlappingImpactSpacing - ImpactDelay);
	}
	return EarliestWindup;
}

void AVolcanoEncounter::BeginGuardianAttack(FGuardianState& Guard, FVolcanoGuardianCue& Cue,
	const ABaseCharacter& Target, const TArray<ABaseCharacter*>& Party, double Now)
{
	ABaseCharacter& Character = *Cue.Character;
	const FProfile& Profile = GetProfile(Cue.Role);
	const bool bSpore = Cue.Role == GuardianRoleType::Sporecaster;
	const FVector Direction = (Target.GetActorLocation() - Character.GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER,
		Character.GetActorForwardVector());
	const FVector Estimate = bSpore ? GuardianFeet(Target) : GuardianFeet(Character) + Direction * Profile.ForwardOffset;
	const FVector Aim = ProjectGuardianAim(Character, Estimate);
	const double WindupAt = GetGuardianWindupTime(Cue, Aim, Target, Party, Now);
	if (WindupAt > Now + UE_SMALL_NUMBER)
	{
		// Delay before showing the warning. Once committed, the pose and landing use the same clock.
		Guard.NextAttackAt = WindupAt;
		UE_LOG(LogVolcanoGuardians, Log, TEXT("%s deferred overlapping impact by %.2f s"), *Character.GetName(), WindupAt - Now);
		return;
	}
	Cue.Aim = Aim;
	Cue.Release = GuardianFeet(Character) + Direction * 62.f + FVector(0, 0, 54.f);
	Character.SetActorRotation(Direction.Rotation());
	Character.ForceNetUpdate();
	Guard.HealthAtWindup = Character.GetHealth();
	Guard.ImpactTarget = &Target;
	Guard.NextAttackAt = Now + GuardianImpactDelay(Cue.Role) + Profile.AttackGap;
	SetGuardianPhase(Cue, Phase::Windup, Now);
	UE_LOG(LogVolcanoGuardians, Log, TEXT("%s %s windup at %s, radius %.0f"), *Character.GetName(),
		*UEnum::GetValueAsString(Cue.Role), *Cue.Aim.ToCompactString(), Profile.Radius);
}

void AVolcanoEncounter::ResolveGuardianAttack(FVolcanoGuardianCue& Cue, const TArray<ABaseCharacter*>& Party, double Now)
{
	ABaseCharacter& Character = *Cue.Character;
	const FProfile& Profile = GetProfile(Cue.Role);
	FAttackModule Hit;
	Hit.Element = Profile.Element;
	const FVector Origin = FVector(Cue.Aim) + FVector(0, 0, 55.f);
	const float Damage = Profile.Damage + (Wave == 2 && Cue.Role != GuardianRoleType::Bulwark ? 2.f : 0.f);
	int32 Hits = 0;
	for (ABaseCharacter* Member : Party)
	{
		const FVector Feet = GuardianFeet(*Member);
		if (!IsWithinGuardianFootprint(Feet, Cue.Aim, Profile.Radius)
			|| !AttackResolution::HasClearPath(Character, *Member, Origin)) continue;
		if (!AttackResolution::ApplyHit(Character, *Member, Hit, Damage)) continue;
		++Hits;
		if (Cue.Role == GuardianRoleType::Bulwark)
			Member->ApplyCombatImpulse((Feet - FVector(Cue.Aim)).GetSafeNormal2D(UE_SMALL_NUMBER, Character.GetActorForwardVector()) * 120.f
				+ FVector(0, 0, 50.f));
	}
	if (Cue.Role == GuardianRoleType::Sporecaster)
		AttackResolution::TriggerHazardsInArea(Character, Origin, Profile.Radius, Profile.Element);
	SetGuardianPhase(Cue, Phase::Recovery, Now);
	UE_LOG(LogVolcanoGuardians, Log, TEXT("%s %s impact, hit %d"), *Character.GetName(),
		*UEnum::GetValueAsString(Cue.Role), Hits);
}

bool AVolcanoEncounter::RequestRestart()
{
	if (!HasAuthority() || bRestarting || (Stage != EVolcanoEncounterStage::Victory && Stage != EVolcanoEncounterStage::Defeat))
		return false;
	bRestarting = GetWorld()->ServerTravel(TEXT("?Restart"));
	return bRestarting;
}
