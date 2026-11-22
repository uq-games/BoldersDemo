#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClassCardWidget.generated.h"

class UClassSelectionData;
class UClassSelectionCatalog;
class ACharacterPreview;
class UButton;
class UImage;
class UTexture2D;
class UMaterialInstanceDynamic;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnClassCardChosen, int32);

UCLASS()
class BOLDERSDEMO_API UClassCardWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(UClassSelectionCatalog* Catalog, int32 Index);
	void SetSelected(bool bSelected);
	FOnClassCardChosen OnChosen;
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<ACharacterPreview> Preview;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandleClicked();
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CardButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Portrait;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Frame;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Caption;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> ChosenBadge;
private:
	void UpdateState();
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PortraitInstance;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> NormalFrame;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> SelectedFrame;
	bool bIsSelected = false;
	int32 CardIndex = INDEX_NONE;
};
