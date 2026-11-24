#pragma once

#include "Combat/Data/AttackLoadout.h"
#include "Components/DecalComponent.h"

namespace CombatGuideStyle
{
	inline constexpr TCHAR MaterialPath[] =
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/M_CombatGuide.M_CombatGuide");

	inline constexpr TCHAR FootprintMaterialPath[] =
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/M_CombatFootprint.M_CombatFootprint");
	inline constexpr TCHAR ReachMaterialPath[] =
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/M_CombatReach.M_CombatReach");
	inline constexpr TCHAR DangerMaterialPath[] =
		TEXT("/Game/BoldersDemo/Assets/Combat/Effects/M_CombatDanger.M_CombatDanger");

	inline constexpr int32 ReachSort = 0;
	inline constexpr int32 FootprintSort = 1;
	inline constexpr int32 DangerSort = 3;
	inline constexpr float DecalBoundary = .88f;

	inline void PlaceGroundDecal(UDecalComponent& Decal, const FVector& Center, const FVector& Heading,
		float ForwardRadius, float SideRadius, float HalfDepth)
	{
		// Damage footprints are tested in XY. A vertical projection preserves that shape over slopes.
		const FVector Right(-Heading.Y, Heading.X, 0.);
		const FVector Size(HalfDepth, SideRadius / DecalBoundary, ForwardRadius / DecalBoundary);
		Decal.SetWorldLocationAndRotation(Center, FRotationMatrix::MakeFromXY(-FVector::UpVector, Right).Rotator());
		if (!Decal.DecalSize.Equals(Size))
		{
			Decal.DecalSize = Size;
			Decal.MarkRenderStateDirty();
		}
	}

	inline FLinearColor ElementColor(EDamageElement Element)
	{
		switch (Element)
		{
		case EDamageElement::Water: return FLinearColor(.06f, .67f, .96f);
		case EDamageElement::Ice: return FLinearColor(.33f, .89f, 1.f);
		case EDamageElement::Lightning: return FLinearColor(.64f, .37f, 1.f);
		case EDamageElement::Fire: return FLinearColor(1.f, .39f, .16f);
		default: return FLinearColor(.94f, .72f, .42f);
		}
	}
}
