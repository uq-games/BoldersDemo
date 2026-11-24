#pragma once

#include "Engine/DataAsset.h"
#include "FoundryTypography.generated.h"

class UImage;
class UTexture2D;

USTRUCT(BlueprintType)
struct FFoundryTextSprite
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> Atlas;
	UPROPERTY(EditAnywhere) FVector4 UV = FVector4(0, 0, 1, 1);
	UPROPERTY(EditAnywhere) float Aspect = 1.f;
};

UCLASS()
class UFoundryTypography : public UDataAsset
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* AssetPath =
		TEXT("/Game/BoldersDemo/Assets/UI/FoundryTypography/DA_FoundryTypography.DA_FoundryTypography");

	const FFoundryTextSprite* Find(const FString& Text) const { return Sprites.Find(Text); }
	bool Apply(UImage* Image, const FString& Text) const;

private:
	UPROPERTY(EditAnywhere, Category="Typography") TMap<FString, FFoundryTextSprite> Sprites;
};
