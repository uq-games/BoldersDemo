#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

namespace CoolingBasinShape
{
	inline constexpr double HalfWidthRatio = .62;
	inline constexpr double CornerRatio = .19;
	inline constexpr int32 CornerCount = 8;

	inline TStaticArray<FVector2D, CornerCount> Outline(double HalfLength, double HalfWidth, double Bevel)
	{
		return {
			FVector2D(HalfLength - Bevel, -HalfWidth), FVector2D(HalfLength, -HalfWidth + Bevel),
			FVector2D(HalfLength, HalfWidth - Bevel), FVector2D(HalfLength - Bevel, HalfWidth),
			FVector2D(-HalfLength + Bevel, HalfWidth), FVector2D(-HalfLength, HalfWidth - Bevel),
			FVector2D(-HalfLength, -HalfWidth + Bevel), FVector2D(-HalfLength + Bevel, -HalfWidth)
		};
	}

	inline bool Contains(const FVector& LocalPoint, double HalfLength)
	{
		const double HalfWidth = HalfLength * HalfWidthRatio;
		const double X = FMath::Abs(LocalPoint.X);
		const double Y = FMath::Abs(LocalPoint.Y);
		return X <= HalfLength && Y <= HalfWidth && X + Y <= HalfLength + HalfWidth - HalfLength * CornerRatio;
	}
}
