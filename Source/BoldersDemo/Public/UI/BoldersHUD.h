#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BoldersHUD.generated.h"
class UTexture2D;
class ABaseCharacter;
UCLASS()
class BOLDERSDEMO_API ABoldersHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
protected:
	void DrawHealthBar(const ABaseCharacter& Character, float X, float Y, float Width, float Height, FLinearColor Color);
	void DrawParty(const TArray<ABaseCharacter*>& Members);
	UPROPERTY(EditDefaultsOnly, Category="Health") TObjectPtr<UTexture2D> HealthBarFrame;
 UPROPERTY(EditDefaultsOnly, Category="Markers") TObjectPtr<UTexture2D> MarkerAtlas;
 UPROPERTY(EditDefaultsOnly, Category="Markers") float MarkerSize = 76.f;
};
