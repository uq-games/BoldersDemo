#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClassSelectionWidget.generated.h"

class UClassCardWidget;
class UClassSelectionCatalog;
class UButton;
class UImage;
class UTextBlock;

UCLASS()
class BOLDERSDEMO_API UClassSelectionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ShowFailure(const FText& Reason);
	UFUNCTION(BlueprintCallable) void ChooseClass(int32 Index);
	UFUNCTION(BlueprintCallable) void ConfirmSelection();
	UFUNCTION(BlueprintCallable) void ClearChoice();
	UPROPERTY(Transient, BlueprintReadOnly) int32 SelectedIndex = INDEX_NONE;
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UClassCardWidget> WarriorCard;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UClassCardWidget> ArcherCard;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UClassCardWidget> MageCard;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ClearButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> BackgroundImage;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> ConfirmImage;
private:
	UPROPERTY(Transient) TObjectPtr<UClassSelectionCatalog> Catalog;
	bool bWaitingForServer = false;
};
