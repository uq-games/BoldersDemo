#if WITH_EDITOR
#include "UI/ClassSelection/ClassSelectionData.h"
#include "UI/ClassSelection/ClassCardWidget.h"
#include "UI/ClassSelection/ClassSelectionWidget.h"
#include "UI/ClassSelection/PreviewAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "K2Node_VariableGet.h"
#include "Factories/AnimBlueprintFactory.h"

#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"

#include "Brushes/SlateColorBrush.h"



#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

namespace ClassSelectionSetup
{
template<class T> T* Node(UEdGraph* Graph)
{
	auto* Result=NewObject<T>(Graph); Graph->AddNode(Result,false,false);
	Result->CreateNewGuid(); Result->PostPlacedNewNode(); Result->AllocateDefaultPins(); return Result;
}
void BuildAnimation(UClassSelectionData* Data)
{
	if (!Data || !Data->IdleAnimation || !Data->HoverAnimation || !Data->SelectedAnimation) return;
	const FString Folder=TEXT("/Game/BoldersDemo/Blueprints/Characters/")+Data->ClassId.ToString()+TEXT("/Preview");
	const FString Name=TEXT("ABP_Preview_")+Data->ClassId.ToString();
	auto* BP=LoadObject<UAnimBlueprint>(nullptr,*(Folder+TEXT("/")+Name));
	if (!BP)
	{
		auto* Factory=NewObject<UAnimBlueprintFactory>(); Factory->ParentClass=UPreviewAnimInstance::StaticClass();
		Factory->TargetSkeleton=Data->IdleAnimation->GetSkeleton(); Factory->PreviewSkeletalMesh=Data->PreviewMesh;
		BP=CastChecked<UAnimBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateAsset(Name,Folder,UAnimBlueprint::StaticClass(),Factory));
		TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
		for (auto* Graph:Graphs)
		{
			if (Graph->GetFName()!=TEXT("AnimGraph")) continue;
			UAnimGraphNode_Root* Root=nullptr;
			for (UEdGraphNode* N:Graph->Nodes) if (auto* R=Cast<UAnimGraphNode_Root>(N)) Root=R;
			if (!Root) continue;
			auto* Blend=Node<UAnimGraphNode_BlendListByInt>(Graph);
			while(Blend->Node.GetBlendTimes().Num()<3) Blend->Node.AddPose();
			auto* Reset=FindFProperty<FEnumProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("ChildUpateMode"));
			check(Reset); Reset->GetUnderlyingProperty()->SetIntPropertyValue(Reset->ContainerPtrToValuePtr<void>(&Blend->Node),int64(EBlendListChildUpdateMode::ResetChildOnActivate));
			Blend->ReconstructNode(); Blend->NodePosX=-250;
			UAnimSequence* Animations[]={Data->IdleAnimation,Data->HoverAnimation,Data->SelectedAnimation};
			for (int32 I=0;I<3;++I)
			{
				auto* Player=Node<UAnimGraphNode_SequencePlayer>(Graph);
				Player->Node.SetSequence(Animations[I]); Player->Node.SetLoopAnimation(I==0);
				Player->NodePosX=-650; Player->NodePosY=I*180;
				check(Graph->GetSchema()->TryCreateConnection(Player->FindPin(TEXT("Pose")),Blend->FindPin(*FString::Printf(TEXT("BlendPose_%d"),I))));
				if(auto* Time=Blend->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I))) Time->DefaultValue=TEXT("0.25");
			}
			auto* Index=NewObject<UK2Node_VariableGet>(Graph); Index->VariableReference.SetSelfMember(TEXT("PoseIndex"));
			Graph->AddNode(Index,false,false); Index->CreateNewGuid(); Index->PostPlacedNewNode(); Index->AllocateDefaultPins();
			Index->NodePosX=-650; Index->NodePosY=-150;
			check(Graph->GetSchema()->TryCreateConnection(Index->GetValuePin(),Blend->FindPin(TEXT("ActiveChildIndex"))));
			check(Graph->GetSchema()->TryCreateConnection(Blend->FindPin(TEXT("Pose")),Root->FindPin(TEXT("Result"))));
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	Data->AnimationClass=BP->GeneratedClass;
	UEditorAssetLibrary::SaveLoadedAsset(BP,false); UEditorAssetLibrary::SaveLoadedAsset(Data,false);
}
template<class T>
T* Widget(UWidgetBlueprint& Blueprint, FName Name)
{
	if (auto* Existing = Cast<T>(Blueprint.WidgetTree->FindWidget(Name))) return Existing;
	auto* Created = Blueprint.WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
	Created->bIsVariable = true;
	Blueprint.OnVariableAdded(Created->GetFName());
	return Created;
}

UCanvasPanelSlot* CanvasSlot(UCanvasPanel& Panel, UWidget& Child)
{
	if (Child.GetParent() == &Panel) return CastChecked<UCanvasPanelSlot>(Child.Slot);
	return Panel.AddChildToCanvas(&Child);
}

void Place(UCanvasPanel& Panel, UWidget& Child, FVector2D Position, FVector2D Size, int32 Layer = 0)
{
	auto* Slot = CanvasSlot(Panel, Child);
	Slot->SetAnchors(FAnchors(0, 0));
	Slot->SetAlignment(FVector2D::ZeroVector);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetZOrder(Layer);
}

void Fill(UCanvasPanel& Panel, UWidget& Child)
{
	auto* Slot = CanvasSlot(Panel, Child);
	Slot->SetAnchors(FAnchors(0, 0, 1, 1));
	Slot->SetOffsets(FMargin(0));
}

const FVector2f ControlsAtlasSize(1672.0f, 941.0f);
const FVector2f LabelsAtlasSize(2172.0f, 724.0f);
constexpr const TCHAR* StylePath = TEXT("/Game/BoldersDemo/Assets/UI/ClassSelection/Style");

UImage* Picture(UWidgetBlueprint& BP, UCanvasPanel& Panel, FName Name, UTexture2D* Texture,
	FVector2D Position, FVector2D Size, int32 Layer = 0, FBox2f UV = FBox2f(FVector2f::ZeroVector, FVector2f(1, 1)))
{
	auto* Image = Widget<UImage>(BP, Name);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Texture)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Texture);
		Brush.SetUVRegion(UV);
		Image->SetBrush(Brush);
	}
	Place(Panel, *Image, Position, Size, Layer);
	return Image;
}

FBox2f AtlasRegion(FVector2f Size, FIntRect Pixels)
{
	return FBox2f(FVector2f(Pixels.Min) / Size, FVector2f(Pixels.Max) / Size);
}

void TransparentButton(UButton& Button)
{
	FButtonStyle Style;
	const FSlateColorBrush Transparent(FLinearColor::Transparent);
	Style.SetNormal(Transparent);
	Style.SetHovered(Transparent);
	Style.SetPressed(Transparent);
	Style.SetDisabled(Transparent);
	Style.NormalPadding = FMargin(0);
	Style.PressedPadding = FMargin(0);
	Button.SetStyle(Style);
	Button.SetCursor(EMouseCursor::Hand);
}

void SaveWidget(UWidgetBlueprint& Blueprint)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
	FKismetEditorUtilities::CompileBlueprint(&Blueprint);
	UEditorAssetLibrary::SaveLoadedAsset(&Blueprint, false);
}

void RemoveWidgets(UWidgetBlueprint& Blueprint, std::initializer_list<const TCHAR*> Names)
{
	for (const TCHAR* Name : Names)
	{
		if (auto* Existing = Blueprint.WidgetTree->FindWidget(FName(Name)))
		{
			if (Blueprint.WidgetTree->RemoveWidget(Existing)) Blueprint.OnVariableRemoved(FName(Name));
		}
	}
}

void BuildCard(UWidgetBlueprint& BP, UTexture2D& Controls)
{
	RemoveWidgets(BP, {TEXT("NameText"), TEXT("RoleText"), TEXT("DescriptionText"), TEXT("SelectionText")});
	auto* Root = Widget<UCanvasPanel>(BP, TEXT("CardRoot"));
	BP.WidgetTree->RootWidget = Root;
	Picture(BP, *Root, TEXT("Frame"), nullptr, {-10, -10}, {396, 574}, -1);
	auto* Button = Widget<UButton>(BP, TEXT("CardButton"));
	TransparentButton(*Button);
	Place(*Root, *Button, FVector2D::ZeroVector, {376, 554});
	auto* Content = Widget<UCanvasPanel>(BP, TEXT("CardContent"));
	Content->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Content->GetParent() != Button) Button->SetContent(Content);
	auto* ContentSlot = CastChecked<UButtonSlot>(Content->Slot);
	ContentSlot->SetPadding(FMargin(0));
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	Picture(BP, *Content, TEXT("Portrait"), nullptr, {13, 10}, {350, 370});
	Picture(BP, *Content, TEXT("Caption"), nullptr, {85, 411}, {206, 96});
	auto* Badge = Picture(BP, *Root, TEXT("ChosenBadge"), &Controls, {237, 13}, {126, 46}, 3,
		AtlasRegion(ControlsAtlasSize, {636, 617, 1036, 766}));
	Badge->SetVisibility(ESlateVisibility::Hidden);
	SaveWidget(BP);
}

void BuildScreen(UWidgetBlueprint& BP, UWidgetBlueprint& Card, UTexture2D& Accents,
	UTexture2D& Controls, UTexture2D& Labels, UTexture2D& PlayerSlots)
{
	RemoveWidgets(BP, {
		TEXT("Crest"), TEXT("GameTitle"), TEXT("Title"), TEXT("Subtitle"), TEXT("PlayerLabel"), TEXT("PlayerPlate"),
		TEXT("ClearLabel"), TEXT("ConfirmLabel"), TEXT("StatusText"), TEXT("StatusRuleLeft"), TEXT("StatusRuleRight")
	});

	auto* Root = Widget<UCanvasPanel>(BP, TEXT("ScreenRoot"));
	BP.WidgetTree->RootWidget = Root;
	auto* Background = Widget<UImage>(BP, TEXT("BackgroundImage"));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(*Root, *Background);
	auto* Scale = Widget<UScaleBox>(BP, TEXT("ResponsiveLayout"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Fill(*Root, *Scale);
	auto* Size = Widget<USizeBox>(BP, TEXT("DesignSize"));
	Size->SetWidthOverride(1600);
	Size->SetHeightOverride(900);
	if (Size->GetParent() != Scale) Scale->SetContent(Size);
	auto* Canvas = Widget<UCanvasPanel>(BP, TEXT("Layout"));
	if (Canvas->GetParent() != Size) Size->SetContent(Canvas);

	// Text and bevels are authored together. The UV rectangles select individual atlas sprites.
	Picture(BP, *Canvas, TEXT("Wordmark"), &Controls, {39, 23}, {255, 45}, 2,
		AtlasRegion(ControlsAtlasSize, {44, 195, 535, 282}));
	const FVector2D PlayerPlatePosition(1420, 21), PlayerPlateSize(139, 47);
	const FVector2f PlayerSlotsAtlasSize(2172.0f, 724.0f);
	auto* HostPlate = Picture(BP, *Canvas, TEXT("HostPlate"), &PlayerSlots, PlayerPlatePosition, PlayerPlateSize, 2,
		AtlasRegion(PlayerSlotsAtlasSize, {111, 212, 975, 512}));
	auto* GuestPlate = Picture(BP, *Canvas, TEXT("GuestPlate"), &PlayerSlots, PlayerPlatePosition, PlayerPlateSize, 2,
		AtlasRegion(PlayerSlotsAtlasSize, {1197, 212, 2061, 512}));
	HostPlate->SetVisibility(ESlateVisibility::Hidden);
	GuestPlate->SetVisibility(ESlateVisibility::Hidden);
	Picture(BP, *Canvas, TEXT("TitleArtwork"), &Labels, {645, 38}, {310, 94}, 2,
		AtlasRegion(LabelsAtlasSize, {90, 103, 655, 273}));

	const FBox2f RuleUV({.5353f, .6822f}, {.9270f, .7917f});
	Picture(BP, *Canvas, TEXT("TitleRuleLeft"), &Accents, {421, 78}, {149, 19}, 1, RuleUV);
	auto* RightRule = Picture(BP, *Canvas, TEXT("TitleRuleRight"), &Accents, {1030, 78}, {149, 19}, 1, RuleUV);
	RightRule->SetRenderScale(FVector2D(-1., 1.));

	const FName CardNames[] = {TEXT("WarriorCard"), TEXT("ArcherCard"), TEXT("MageCard")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CardNames); ++Index)
	{
		auto* Entry = BP.WidgetTree->FindWidget(CardNames[Index]);
		if (!Entry)
		{
			Entry = BP.WidgetTree->ConstructWidget<UClassCardWidget>(Card.GeneratedClass.Get(), CardNames[Index]);
			Entry->bIsVariable = true;
			BP.OnVariableAdded(Entry->GetFName());
		}
		Place(*Canvas, *Entry, FVector2D(216 + Index * 396, 156), FVector2D(376, 554));
	}

	auto* Waiting = Picture(BP, *Canvas, TEXT("WaitingArtwork"), &Labels, {701, 710}, {198, 52}, 1,
		AtlasRegion(LabelsAtlasSize, {851, 478, 1333, 605}));
	auto* Failure = Picture(BP, *Canvas, TEXT("FailureArtwork"), &Labels, {667.5, 710}, {265, 52}, 1,
		AtlasRegion(LabelsAtlasSize, {1491, 480, 2137, 606}));
	Waiting->SetVisibility(ESlateVisibility::Hidden);
	Failure->SetVisibility(ESlateVisibility::Hidden);

	const FVector2D ConfirmPosition(595.5, 761), ConfirmSize(409, 108);
	Picture(BP, *Canvas, TEXT("ConfirmImage"), &Controls, ConfirmPosition, ConfirmSize, 1,
		AtlasRegion(ControlsAtlasSize, {553, 169, 1118, 321}));
	auto* Confirm = Widget<UButton>(BP, TEXT("ConfirmButton"));
	TransparentButton(*Confirm);
	Place(*Canvas, *Confirm, ConfirmPosition, ConfirmSize, 2);

	const FVector2D ClearPosition(39, 787), ClearSize(175, 54);
	Picture(BP, *Canvas, TEXT("ClearPlate"), &Controls, ClearPosition, ClearSize, 1,
		AtlasRegion(ControlsAtlasSize, {1150, 169, 1641, 321}));
	auto* Clear = Widget<UButton>(BP, TEXT("ClearButton"));
	TransparentButton(*Clear);
	Place(*Canvas, *Clear, ClearPosition, ClearSize, 2);
	SaveWidget(BP);
}

void Layout()
{
	auto LoadStyle = [](const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *(FString(StylePath) / Name));
	};
	auto* Accents = LoadStyle(TEXT("T_SelectionAccents"));
	auto* Controls = LoadStyle(TEXT("T_SelectionTextControls"));
	auto* Labels = LoadStyle(TEXT("T_SelectionTextLabels"));
	auto* PlayerSlots = LoadStyle(TEXT("T_SelectionPlayerSlots"));
	auto* Card = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/BoldersDemo/Blueprints/UI/ClassSelection/WBP_ClassCard"));
	auto* Screen = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/BoldersDemo/Blueprints/UI/ClassSelection/WBP_ClassSelection"));
	if (!Accents || !Controls || !Labels || !PlayerSlots || !Card || !Screen)
	{
		UE_LOG(LogTemp, Warning, TEXT("Class selection layout needs the style assets and both widget blueprints."));
		return;
	}
	BuildCard(*Card, *Controls);
	BuildScreen(*Screen, *Card, *Accents, *Controls, *Labels, *PlayerSlots);
}

void Build()
{
	for (const TCHAR* Role : {TEXT("Warrior"), TEXT("Archer"), TEXT("Mage")})
		BuildAnimation(LoadObject<UClassSelectionData>(nullptr,
			*(FString(TEXT("/Game/BoldersDemo/Assets/UI/ClassSelection/DA_Class_")) + Role)));
	Layout();
}
}
static FAutoConsoleCommand LayoutClassSelection(TEXT("Bolders.LayoutClassSelection"),
	TEXT("Apply the class-selection art and layout to the existing widget blueprints."),
	FConsoleCommandDelegate::CreateStatic(&ClassSelectionSetup::Layout));
static FAutoConsoleCommand BuildClassSelection(TEXT("Bolders.BuildClassSelection"),TEXT("Build class preview animation graphs and selection widget layouts."),FConsoleCommandDelegate::CreateStatic(&ClassSelectionSetup::Build));
#endif
