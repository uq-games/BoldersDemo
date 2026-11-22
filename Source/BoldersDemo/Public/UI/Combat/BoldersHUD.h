#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BoldersHUD.generated.h"

class UTexture2D;
class ABaseCharacter;

USTRUCT(BlueprintType)
struct FCombatHUDIcon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Icon") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Icon") FVector4 UV = FVector4(0, 0, 1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Icon") bool bAvailable = false;
};

USTRUCT(BlueprintType)
struct FCombatHUDLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout") TSubclassOf<ABaseCharacter> CharacterClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout") TObjectPtr<UTexture2D> Atlas;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout") TArray<FCombatHUDIcon> Icons;
};

UCLASS()
class BOLDERSDEMO_API ABoldersHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Health") TObjectPtr<UTexture2D> HealthBarFrame;
	UPROPERTY(EditDefaultsOnly, Category="Health") FVector4 HealthBarFrameUV = FVector4(20.0/2172, 225.0/724, 2152.0/2172, 495.0/724);
	UPROPERTY(EditDefaultsOnly, Category="Health") FVector4 HealthBarFillBounds = FVector4(.075, .25, .925, .75);
	UPROPERTY(EditDefaultsOnly, Category="Markers") TObjectPtr<UTexture2D> MarkerAtlas;
	UPROPERTY(EditDefaultsOnly, Category="Markers") float MarkerSize = 76.f;
	UPROPERTY(EditDefaultsOnly, Category="Attacks") TArray<FCombatHUDLoadout> Loadouts;

private:
	struct FHealthPresentation
	{
		float PreviousRatio = 1.0f;
		float TrailStart = 1.0f;
		double DamageTime = 0.0;
	};

	float GetDamageTrail(const ABaseCharacter& Character, float Ratio);
	void DrawHealthBar(const ABaseCharacter& Character, float X, float Y, float Width, float Height, FLinearColor Color);
	void DrawParty(const TArray<ABaseCharacter*>& Members, const ABaseCharacter* LocalCharacter, float Scale);
	void DrawEnemyHealth(const ABaseCharacter& Character, float Scale);
	void DrawAttackIcons(const ABaseCharacter& Character, float Scale);

	TMap<TWeakObjectPtr<const ABaseCharacter>, FHealthPresentation> HealthPresentation;
};
