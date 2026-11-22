#include "UI/ClassSelection/ClassSelectionWidget.h"
#include "UI/ClassSelection/ClassCardWidget.h"
#include "UI/ClassSelection/ClassSelectionData.h"
#include "Player/BoldersPlayerController.h"
#include "Components/Button.h"
#include "Components/Image.h"

void UClassSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	auto* PC=Cast<ABoldersPlayerController>(GetOwningPlayer());
	Catalog=PC ? PC->GetSelectionCatalog() : nullptr;
	if (!Catalog) return;
	BackgroundImage->SetBrushFromTexture(Catalog->Background);
	ConfirmImage->SetBrushFromTexture(Catalog->ConfirmPlate);
	const int32 PlayerSlot = PC->GetPlayerSlot();
	if (HostPlate) HostPlate->SetVisibility(PlayerSlot == 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (GuestPlate) GuestPlate->SetVisibility(PlayerSlot == 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
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
	SetStatusArtwork(nullptr);
	ConfirmImage->SetColorAndOpacity(FLinearColor::White);
	ConfirmButton->SetIsEnabled(true);
}
void UClassSelectionWidget::ClearChoice()
{
	if (bWaitingForServer) return;
	SelectedIndex=INDEX_NONE;
	WarriorCard->SetSelected(false); ArcherCard->SetSelected(false); MageCard->SetSelected(false);
	SetStatusArtwork(nullptr);
	ConfirmImage->SetColorAndOpacity(FLinearColor(.62f,.60f,.56f));
	ConfirmButton->SetIsEnabled(false);
}
void UClassSelectionWidget::ConfirmSelection()
{
	if (bWaitingForServer || !Catalog || !Catalog->Classes.IsValidIndex(SelectedIndex)) return;
	if (auto* PC=Cast<ABoldersPlayerController>(GetOwningPlayer()))
	{
		bWaitingForServer=true;
		ConfirmButton->SetIsEnabled(false);
		SetStatusArtwork(WaitingArtwork);
		PC->SubmitClassSelection(Catalog->Classes[SelectedIndex]->ClassId);
	}
}
void UClassSelectionWidget::ShowFailure(const FText&)
{
	bWaitingForServer=false;
	SetStatusArtwork(FailureArtwork);
	ConfirmButton->SetIsEnabled(SelectedIndex!=INDEX_NONE);
}

void UClassSelectionWidget::SetStatusArtwork(UImage* ActiveArtwork)
{
	for (UImage* Artwork : {WaitingArtwork.Get(), FailureArtwork.Get()})
	{
		if (Artwork) Artwork->SetVisibility(Artwork == ActiveArtwork ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
