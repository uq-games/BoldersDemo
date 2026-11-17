#include "UI/ClassSelectionWidget.h"
#include "UI/ClassCardWidget.h"
#include "UI/ClassSelectionData.h"
#include "Player/BoldersPlayerController.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UClassSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	auto* PC=Cast<ABoldersPlayerController>(GetOwningPlayer());
	Catalog=PC ? PC->GetSelectionCatalog() : nullptr;
	if (!Catalog) return;
	BackgroundImage->SetBrushFromTexture(Catalog->Background);
	ConfirmImage->SetBrushFromTexture(Catalog->ConfirmPlate);
	TArray<UClassCardWidget*> Cards={WarriorCard,ArcherCard,MageCard};
	for (int32 I=0;I<Cards.Num();++I)
	{
		Cards[I]->OnChosen.RemoveAll(this);
		Cards[I]->OnChosen.AddUObject(this,&ThisClass::ChooseClass);
		Cards[I]->Configure(Catalog,I);
	}
	ConfirmButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ConfirmSelection);
	ClearButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ClearChoice);
	ClearChoice();
}
void UClassSelectionWidget::ChooseClass(int32 Index)
{
	if (bWaitingForServer || !Catalog || !Catalog->Classes.IsValidIndex(Index) || !Catalog->Classes[Index]) return;
	SelectedIndex=Index;
	WarriorCard->SetSelected(Index==0); ArcherCard->SetSelected(Index==1); MageCard->SetSelected(Index==2);
	StatusText->SetText(FText::Format(FText::FromString(TEXT("当前选择：{0}")),Catalog->Classes[Index]->DisplayName));
	ConfirmButton->SetIsEnabled(true);
}
void UClassSelectionWidget::ClearChoice()
{
	if (bWaitingForServer) return;
	SelectedIndex=INDEX_NONE;
	WarriorCard->SetSelected(false); ArcherCard->SetSelected(false); MageCard->SetSelected(false);
	StatusText->SetText(FText::FromString(TEXT("请选择一个职业")));
	ConfirmButton->SetIsEnabled(false);
}
void UClassSelectionWidget::ConfirmSelection()
{
	if (bWaitingForServer || !Catalog || !Catalog->Classes.IsValidIndex(SelectedIndex)) return;
	if (auto* PC=Cast<ABoldersPlayerController>(GetOwningPlayer()))
	{
		bWaitingForServer=true;
		ConfirmButton->SetIsEnabled(false);
		StatusText->SetText(FText::FromString(TEXT("正在进入冒险……")));
		PC->SubmitClassSelection(Catalog->Classes[SelectedIndex]->ClassId);
	}
}
void UClassSelectionWidget::ShowFailure(const FText& Reason)
{
	bWaitingForServer=false;
	StatusText->SetText(Reason);
	ConfirmButton->SetIsEnabled(SelectedIndex!=INDEX_NONE);
}
