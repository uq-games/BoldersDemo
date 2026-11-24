#include "UI/ClassSelection/ClassCardWidget.h"
#include "UI/ClassSelection/ClassSelectionData.h"
#include "UI/ClassSelection/CharacterPreview.h"
#include "UI/FoundryTypography.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
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
	if (Caption && Catalog->CaptionAtlas)
	{
		const FVector4& UV = Data->CaptionUV;
		FSlateBrush Brush = Caption->GetBrush();
		Brush.SetResourceObject(Catalog->CaptionAtlas);
		Brush.SetUVRegion(FBox2f(FVector2f(UV.X, UV.Y), FVector2f(UV.Z, UV.W)));
		Caption->SetBrush(Brush);
	}
	if (const auto* Art = LoadObject<UFoundryTypography>(nullptr, UFoundryTypography::AssetPath, nullptr, LOAD_NoWarn))
	{
		const TCHAR* RoleCaption[] = {TEXT("Select.战士"), TEXT("Select.弓手"), TEXT("Select.法师")};
		if (Index >= 0 && Index < UE_ARRAY_COUNT(RoleCaption)) Art->Apply(Caption, RoleCaption[Index]);
		Art->Apply(ChosenBadge, TEXT("Select.✓ 已选择"));
	}
	NormalFrame = Catalog->CardFrame;
	SelectedFrame = Catalog->SelectedCardFrame;
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
	if (ChosenBadge) ChosenBadge->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Frame->SetBrushFromTexture(bIsSelected && SelectedFrame ? SelectedFrame : NormalFrame);
	Frame->SetColorAndOpacity(bIsSelected ? FLinearColor(1.f, .94f, .82f)
		: bHover ? FLinearColor(1.10f, 1.06f, .99f) : FLinearColor::White);
}
void UClassCardWidget::NativeDestruct()
{
	OnChosen.Clear();
	if (IsValid(Preview)) Preview->Destroy();
	Preview=nullptr; PortraitInstance=nullptr;
	Super::NativeDestruct();
}
