#include "UI/ClassCardWidget.h"
#include "UI/ClassSelectionData.h"
#include "UI/CharacterPreview.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

void UClassCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CardButton->OnHovered.AddUniqueDynamic(this,&ThisClass::HandleHovered);
	CardButton->OnUnhovered.AddUniqueDynamic(this,&ThisClass::HandleUnhovered);
	CardButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleClicked);
}
void UClassCardWidget::Configure(UClassSelectionCatalog* Catalog, int32 Index)
{
	if (!Catalog || !Catalog->Classes.IsValidIndex(Index) || !Catalog->Classes[Index]) return;
	if (IsValid(Preview)) Preview->Destroy();
	CardIndex = Index;
	const auto* Data = Catalog->Classes[Index].Get();
	NameText->SetText(Data->DisplayName); RoleText->SetText(Data->RoleText); DescriptionText->SetText(Data->Description);
	Frame->SetBrushFromTexture(Catalog->CardFrame);
	FActorSpawnParameters Params; Params.Owner = GetOwningPlayer();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Preview = GetWorld()->SpawnActor<ACharacterPreview>(Catalog->PreviewActorClass,
		FVector(Index*1500.,0.,-100000.), FRotator::ZeroRotator, Params);
	if (Preview)
	{
		Preview->Configure(Catalog->Classes[Index]);
		PortraitInstance = UMaterialInstanceDynamic::Create(Catalog->PortraitMaterial, this);
		if (PortraitInstance)
		{
			PortraitInstance->SetTextureParameterValue(TEXT("Portrait"),Preview->RenderTarget);
			Portrait->SetBrushFromMaterial(PortraitInstance);
		}
	}
	SetSelected(false);
}
void UClassCardWidget::SetSelected(bool bSelected) { bIsSelected=bSelected; UpdateState(); }
void UClassCardWidget::HandleHovered() { UpdateState(); }
void UClassCardWidget::HandleUnhovered() { UpdateState(); }
void UClassCardWidget::HandleClicked() { OnChosen.Broadcast(CardIndex); }
void UClassCardWidget::UpdateState()
{
	const bool bHover=CardButton->IsHovered();
	if (Preview) Preview->SetDisplayState(bIsSelected ? 2 : bHover ? 1 : 0);
	SelectionText->SetText(bIsSelected ? FText::FromString(TEXT("✓ 已选择")) : FText::GetEmpty());
	Frame->SetColorAndOpacity(bIsSelected ? FLinearColor(.35f,1.f,.9f,1.f) : bHover ? FLinearColor(1.f,1.f,.9f,1.f) : FLinearColor(.8f,.8f,.8f,.85f));
}
void UClassCardWidget::NativeDestruct()
{
	OnChosen.Clear();
	if (IsValid(Preview)) Preview->Destroy();
	Preview=nullptr; PortraitInstance=nullptr;
	Super::NativeDestruct();
}
