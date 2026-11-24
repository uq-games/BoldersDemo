#include "Game/VolcanoEncounter.h"
#include "Game/VolcanoGuardianProfile.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Characters/BaseCharacter.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DecalActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	using GuardianRoleType = EVolcanoGuardianRole;
	using Phase = EVolcanoGuardianPhase;
	using namespace VolcanoGuardians;

	const FName GuardianSlot = TEXT("DefaultSlot");
	const FName WarningTag = TEXT("VolcanoGuardianWarning");
	const FName SporeTag = TEXT("VolcanoGuardianSpore");

	struct FGuardianWarningAppearance
	{
		FLinearColor Tint;
		float Style;
	};

	FGuardianWarningAppearance GetWarningAppearance(GuardianRoleType GuardianRole)
	{
		switch (GuardianRole)
		{
		case GuardianRoleType::Sporecaster: return {FLinearColor(1.f, .55f, .14f), 1.f};
		case GuardianRoleType::Bulwark: return {FLinearColor(1.f, .23f, .095f), 2.f};
		default: return {FLinearColor(1.f, .35f, .12f), 0.f};
		}
	}

	UAnimMontage* PlayGuardianMontage(UAnimInstance* Anim, UAnimSequence* Sequence, float Rate, float StartAt)
	{
		if (!Anim || !Sequence || StartAt >= Sequence->GetPlayLength() / FMath::Max(Sequence->RateScale, .01f)) return nullptr;
		UAnimMontage* Montage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, GuardianSlot, .08f, .13f, Rate, 1, -1.f, StartAt);
		// The short forward and upward offsets belong to the local performance. AI owns capsule movement.
		if (Montage)
			if (auto* Instance = Anim->GetActiveInstanceForMontage(Montage)) Instance->PushDisableRootMotion();
		return Montage;
	}
}

void AVolcanoEncounter::FGuardianVisual::ClearMeshes()
{
	if (auto* Mark = Warning.Get()) Mark->Destroy();
	if (auto* Missile = Spore.Get()) Missile->Destroy();
	Warning.Reset();
	Spore.Reset();
}

void AVolcanoEncounter::EndPlay(const EEndPlayReason::Type Reason)
{
	for (auto& Pair : GuardianVisuals) Pair.Value.ClearMeshes();
	GuardianVisuals.Reset();
	Super::EndPlay(Reason);
}

UAnimSequence* AVolcanoEncounter::GetGuardianAttackAnimation(EVolcanoGuardianRole GuardianRole) const
{
	if (GuardianRole == GuardianRoleType::Sporecaster) return SporecasterAttackAnimation;
	if (GuardianRole == GuardianRoleType::Bulwark) return BulwarkAttackAnimation;
	return ScrapperAttackAnimation;
}

AStaticMeshActor* AVolcanoEncounter::SpawnGuardianMesh(UStaticMesh* Mesh, UMaterialInterface* Material,
	const FVector& Position, const FVector& Scale, FName Tag, float Lifespan) const
{
	if (!Mesh) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator, Params);
	if (!Actor) return nullptr;
	Actor->Tags.Add(Tag);
	Actor->SetActorEnableCollision(false);
	Actor->SetLifeSpan(Lifespan);
	auto* Surface = Actor->GetStaticMeshComponent();
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCastShadow(false);
	Surface->SetStaticMesh(Mesh);
	if (Material) Surface->SetMaterial(0, Material);
	else Surface->SetVisibility(false);
	Actor->SetActorScale3D(Scale);
	return Actor;
}

void AVolcanoEncounter::UpdateGuardianVisuals(double Now)
{
	for (auto It = GuardianVisuals.CreateIterator(); It; ++It)
	{
		if (FindGuardianCue(It.Key().Get())) continue;
		It.Value().ClearMeshes();
		It.RemoveCurrent();
	}
	for (const FVolcanoGuardianCue& Cue : GuardianCues)
	{
		if (!IsValid(Cue.Character) || !Cue.Character->HasActorBegunPlay()) continue;
		FGuardianVisual& Visual = GuardianVisuals.FindOrAdd(TWeakObjectPtr<ABaseCharacter>(Cue.Character.Get()));
		if (!Visual.bStyleApplied) Visual.bStyleApplied = ApplyGuardianStyle(Cue);
		const float Age = static_cast<float>(FMath::Max(0.0, Now - Cue.StartedAt));
		if (Visual.CueStartedAt != Cue.StartedAt)
		{
			Visual.CueStartedAt = Cue.StartedAt;
			ApplyGuardianVisual(Cue, Visual, Age);
		}
		if (Cue.Phase == Phase::Windup)
			UpdateGuardianAttackPose(Cue, Visual, Age);
		else if (Cue.Phase == Phase::InFlight)
		{
			if (auto* Spore = Visual.Spore.Get())
				Spore->SetActorLocation(SporePosition(Cue.Release, FVector(Cue.Aim) + FVector(0, 0, 14.f),
					FMath::Clamp(Age / SporeTravel, 0.f, 1.f)));
		}
	}
}

bool AVolcanoEncounter::ApplyGuardianStyle(const FVolcanoGuardianCue& Cue) const
{
	auto* Mesh = Cue.Character->GetMesh();
	const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return false;
	if (Asset->GetFName() != TEXT("Mushroom_SK")) return true;
	const int32 Index = static_cast<int32>(Cue.Role) - 1;
	if (!GuardianSkins.IsValidIndex(Index)) return true;

	if (UMaterialInterface* Skin = GuardianSkins[Index].Get())
	{
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			auto* Material = Mesh->GetMaterial(Slot);
			const auto* Source = Material ? Material->GetBaseMaterial() : nullptr;
			if (Source && (Source->GetFName() == TEXT("DefaultPBR_MAT") || Source->GetFName() == TEXT("DefaultPolyart_MAT")))
				Mesh->SetMaterial(Slot, Skin);
		}
	}
	// The source cap's width is along mesh X. The collider and ground pivot remain fixed.
	if (Cue.Role == GuardianRoleType::Bulwark)
		Mesh->SetRelativeScale3D(Mesh->GetRelativeScale3D() * FVector(1.17f, 1.09f, 1.04f));
	else if (Cue.Role == GuardianRoleType::Sporecaster)
		Mesh->SetRelativeScale3D(Mesh->GetRelativeScale3D() * FVector(.91f, .94f, 1.03f));
	return true;
}

void AVolcanoEncounter::PlayGuardianCueAudio(const FVolcanoGuardianCue& Cue, float Age) const
{
	if (Age > .19f) return;
	const bool bSpore = Cue.Role == GuardianRoleType::Sporecaster;
	const bool bWarning = Cue.Phase == Phase::Windup;
	const bool bRelease = bSpore ? Cue.Phase == Phase::InFlight : Cue.Phase == Phase::Recovery;
	if (!bWarning && !bRelease) return;
	const auto& Clips = bWarning ? GuardianWarnings : GuardianContacts;
	const int32 Index = static_cast<int32>(Cue.Role) - 1;
	if (!Clips.IsValidIndex(Index) || !Clips[Index]) return;
	const FVector Source = bWarning ? Cue.Character->GetActorLocation() : bSpore ? FVector(Cue.Release) : FVector(Cue.Aim);
	UGameplayStatics::PlaySoundAtLocation(this, Clips[Index], Source, bWarning ? .55f : .67f, 1.f,
		FMath::Min(Age, .07f), GuardianAttenuation);
}

void AVolcanoEncounter::ApplyGuardianVisual(const FVolcanoGuardianCue& Cue, FGuardianVisual& Visual, float Age)
{
	const FProfile& Profile = GetProfile(Cue.Role);
	const bool bSpore = Cue.Role == GuardianRoleType::Sporecaster;
	const bool bWarning = Cue.Phase == Phase::Windup || Cue.Phase == Phase::InFlight;
	auto* Anim = Cue.Character->GetMesh()->GetAnimInstance();
	PlayGuardianCueAudio(Cue, Age);
	if (!bWarning) Visual.ClearMeshes();
	if (Cue.Phase == Phase::Emerging)
	{
		const float WakeRate = .74f;
		Visual.Montage = PlayGuardianMontage(Anim, GuardianWakeAnimation, WakeRate, Age * WakeRate);
		return;
	}
	if (Cue.Phase == Phase::Interrupted || Cue.Phase == Phase::Defeated || Cue.Phase == Phase::Idle)
	{
		if (Anim && Visual.Montage.IsValid()) Anim->Montage_Stop(.1f, Visual.Montage.Get());
		Visual.Montage.Reset();
		if (Cue.Phase == Phase::Defeated) Cue.Character->SetActorEnableCollision(false);
		return;
	}
	if (Cue.Phase == Phase::Recovery)
	{
		UNiagaraSystem* Impact = bSpore ? SporeImpact.Get()
			: Cue.Role == GuardianRoleType::Bulwark ? BulwarkImpact.Get() : ScrapperImpact.Get();
		if (Impact && Age < .22f)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Impact, FVector(Cue.Aim) + FVector(0, 0, 18.f),
				FRotator::ZeroRotator, FVector(Profile.Radius / 150.f));
		return;
	}

	const float WarningLifetime = Profile.Windup + (bSpore ? SporeTravel : 0.f);
	const float WarningAge = Cue.Phase == Phase::InFlight ? Profile.Windup + Age : Age;
	if (TelegraphDecal && !Visual.Warning.IsValid() && WarningAge < WarningLifetime)
	{
		const FVector Center = Cue.Aim;
		FActorSpawnParameters Params;
		Params.CustomPreSpawnInitialization = [DecalMaterial = TelegraphDecal.Get()](AActor* Actor)
		{
			CastChecked<ADecalActor>(Actor)->GetDecal()->SetDecalMaterial(DecalMaterial);
		};
		auto* Mark = GetWorld()->SpawnActor<ADecalActor>(Center, FRotator::ZeroRotator, Params);
		Visual.Warning = Mark;
		if (Mark)
		{
			Mark->Tags.Add(WarningTag);
			Mark->SetLifeSpan(WarningLifetime - WarningAge + .12f);
			auto* Decal = Mark->GetDecal();
			Decal->SetSortOrder(CombatGuideStyle::DangerSort);
			Decal->FadeScreenSize = 0.f;
			CombatGuideStyle::PlaceGroundDecal(*Decal, Center, FVector::ForwardVector,
				Profile.Radius, Profile.Radius, GuardianFootprintHeight);
			if (auto* Material = Decal->CreateDynamicMaterialInstance())
			{
				const FGuardianWarningAppearance Appearance = GetWarningAppearance(Cue.Role);
				Material->SetScalarParameterValue(TEXT("StartTime"), GetWorld()->GetTimeSeconds() - WarningAge);
				Material->SetScalarParameterValue(TEXT("Lifetime"), WarningLifetime);
				Material->SetVectorParameterValue(TEXT("RingColor"), Appearance.Tint);
				Material->SetScalarParameterValue(TEXT("MarkStyle"), Appearance.Style);
			}
		}
	}
	if (Cue.Phase == Phase::Windup)
	{
		Visual.bAttackPoseStarted = false;
		if (Age < .24f) Visual.Montage = PlayGuardianMontage(Anim, GuardianBraceAnimation, 1.f, Age);
	}
	else if (Age < SporeTravel)
	{
		const FVector Spawn = SporePosition(Cue.Release, FVector(Cue.Aim) + FVector(0, 0, 14.f), Age / SporeTravel);
		Visual.Spore = SpawnGuardianMesh(SporeMesh, SporeMaterial, Spawn, FVector(.56f), SporeTag, SporeTravel - Age + .12f);
	}
}

void AVolcanoEncounter::UpdateGuardianAttackPose(const FVolcanoGuardianCue& Cue, FGuardianVisual& Visual, float Age) const
{
	if (Visual.bAttackPoseStarted) return;
	UAnimSequence* Sequence = GetGuardianAttackAnimation(Cue.Role);
	auto* Anim = Cue.Character->GetMesh()->GetAnimInstance();
	if (!Sequence || !Anim) return;
	const FProfile& Profile = GetProfile(Cue.Role);
	const float ContactInMontage = Profile.ContactFrameTime / FMath::Max(Sequence->RateScale, .01f);
	const float LeadIn = FMath::Max(0.f, Profile.Windup - ContactInMontage / Profile.PlayRate);
	if (Age < LeadIn || Age >= Profile.Windup + .02f) return;
	Visual.Montage = PlayGuardianMontage(Anim, Sequence, Profile.PlayRate, (Age - LeadIn) * Profile.PlayRate);
	Visual.bAttackPoseStarted = Visual.Montage.IsValid();
}
