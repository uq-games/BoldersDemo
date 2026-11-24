#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BoldersHUD.generated.h"

class UTexture2D;
class UFoundryTypography;
class ABaseCharacter;
class AVolcanoEncounter;
class AVolcanoWaySign;
class ABoldersPlayerController;
class UDecalComponent;
class UMaterialInstanceDynamic;
enum class EDamageElement : uint8;

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
	ABoldersHUD();
	virtual void PostRender() override;
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	bool CloseIslandMap();
	bool CloseWaySign();
	void ReadNearestWaySign();
	bool IsModalOpen() const { return bIslandMapExpanded || ReadingSign.IsValid(); }

protected:
	UPROPERTY(EditDefaultsOnly, Category="Health") TObjectPtr<UTexture2D> HealthBarFrame;
	UPROPERTY(EditDefaultsOnly, Category="Health") FVector4 HealthBarFrameUV = FVector4(20.0/2172, 225.0/724, 2152.0/2172, 495.0/724);
	UPROPERTY(EditDefaultsOnly, Category="Health") FVector4 HealthBarFillBounds = FVector4(.075, .25, .925, .75);
	UPROPERTY(EditDefaultsOnly, Category="Markers") TObjectPtr<UTexture2D> MarkerAtlas;
	UPROPERTY(EditDefaultsOnly, Category="Markers") float MarkerSize = 76.f;
	UPROPERTY(EditDefaultsOnly, Category="Attacks") TArray<FCombatHUDLoadout> Loadouts;
	UPROPERTY(EditDefaultsOnly, Category="Minimap") TObjectPtr<UTexture2D> VolcanoMinimap;
	UPROPERTY(EditDefaultsOnly, Category="Minimap") TObjectPtr<UTexture2D> MinimapPlayerOrbs;

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
	void DrawAttackIcons(const ABaseCharacter& Character, float Scale, EDamageElement DipElement, bool bInVolcano);
	void DrawCombatControls(const ABaseCharacter& Character, const ABoldersPlayerController& PC, float Scale,
		EDamageElement DipElement, const FVector& DipContact);
	void DrawIslandMinimap(const AVolcanoEncounter& Encounter, const ABaseCharacter& Character,
		const TArray<ABaseCharacter*>& Party, float Scale);
	void DrawMinimapOrb(int32 PlayerSlot, const FVector2D& Center, float Radius);
	void DrawMinimapPlayerMarker(int32 PlayerSlot, const FVector2D& Point, float Radius, const FVector2D& Heading);
	void UpdateAttackGuide(const ABaseCharacter* Character, const ABoldersPlayerController* PC, bool bVisible);
	void DrawVolcano(const AVolcanoEncounter& Encounter, const ABoldersPlayerController& PC, float Scale);
	void DrawWaySign(const ABaseCharacter& Character, float Scale);
	void DrawLabel(UCanvas& DrawCanvas, const FString& Text, const FVector2D& Position, float Scale,
		const FLinearColor& Color, bool bCentered = false);
	void DrawKit(const FVector4& Crop, const FVector2D& Position, const FVector2D& Size,
		FLinearColor Tint = FLinearColor::White);

	UPROPERTY() TObjectPtr<UMaterialInterface> AttackGuideMaterial;
	UPROPERTY() TObjectPtr<UTexture2D> FoundryLifeFrame;
	UPROPERTY() TObjectPtr<UTexture2D> FoundryHudKit;
	UPROPERTY() TObjectPtr<UTexture2D> BladeDipArt;
	UPROPERTY() TObjectPtr<UFoundryTypography> Typography;
	UPROPERTY(Transient) TObjectPtr<UDecalComponent> AttackGuide;
	UPROPERTY(Transient) TObjectPtr<UDecalComponent> ImpactGuide;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AttackGuideMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ImpactGuideMID;
	bool bIslandMapExpanded = false;
	TWeakObjectPtr<const AVolcanoWaySign> NearbySign;
	TWeakObjectPtr<const AVolcanoWaySign> ReadingSign;
	TMap<TWeakObjectPtr<const ABaseCharacter>, FHealthPresentation> HealthPresentation;
};
