#pragma once

#include "Combat/Data/AttackLoadout.h"
#include "Game/VolcanoEncounter.h"

namespace VolcanoGuardians
{
	struct FProfile
	{
		float Reach;
		float Radius;
		float ForwardOffset;
		float Windup;
		float ContactFrameTime;
		float PlayRate;
		float Recovery;
		float AttackGap;
		float WalkSpeed;
		float Health;
		float Damage;
		EDamageElement Element;
	};

	constexpr float SpawnTime = 1.0f;
	constexpr float GuardianFootprintHeight = 94.f;
	constexpr float SporeTravel = .52f;
	constexpr float SporeArcHeight = 135.f;
	constexpr float InterruptTime = .65f;
	constexpr float BulwarkArmor = .25f;
	constexpr float BulwarkExposedArmor = -.20f;

	inline const FProfile& GetProfile(EVolcanoGuardianRole Role)
	{
		// Contact times sampled from the authored clips: forward thrust, cap flick, then leap landing.
		static constexpr FProfile Scrapper{225.f, 130.f, 113.f, .78f, .417f, .96f, .70f, 1.65f, 245.f, 64.f, 13.f, EDamageElement::Physical};
		static constexpr FProfile Sporecaster{795.f, 152.f, 0.f, .93f, .450f, .92f, .67f, 2.20f, 194.f, 57.f, 15.f, EDamageElement::Fire};
		static constexpr FProfile Bulwark{255.f, 224.f, 44.f, 1.24f, .667f, .80f, 1.35f, 1.60f, 169.f, 109.f, 22.f, EDamageElement::Physical};
		if (Role == EVolcanoGuardianRole::Sporecaster) return Sporecaster;
		if (Role == EVolcanoGuardianRole::Bulwark) return Bulwark;
		return Scrapper;
	}

	inline FVector SporePosition(const FVector& Launch, const FVector& Land, float Progress)
	{
		return FMath::Lerp(Launch, Land, Progress)
			+ FVector::UpVector * (4.f * SporeArcHeight * Progress * (1.f - Progress));
	}
}
