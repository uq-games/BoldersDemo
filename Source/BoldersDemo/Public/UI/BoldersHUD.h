#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BoldersHUD.generated.h"
class UTexture2D;
UCLASS()
class BOLDERSDEMO_API ABoldersHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
protected:
 UPROPERTY(EditDefaultsOnly, Category="Markers") TObjectPtr<UTexture2D> MarkerAtlas;
 UPROPERTY(EditDefaultsOnly, Category="Markers") float MarkerSize = 76.f;
};
