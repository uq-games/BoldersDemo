#if WITH_EDITOR
#include "UI/ClassSelectionData.h"
#include "UI/ClassCardWidget.h"
#include "UI/ClassSelectionWidget.h"
#include "UI/PreviewAnimInstance.h"
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
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Engine/Font.h"
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
template<class T> T* Widget(UWidgetBlueprint* BP,FName Name)
{
	auto* Result=BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),Name); Result->bIsVariable=true; return Result;
}
void Position(UCanvasPanel* Panel,UWidget* Widget,float X,float Y,float W,float H)
{
	auto* Slot=Panel->AddChildToCanvas(Widget); Slot->SetPosition(FVector2D(X,Y)); Slot->SetSize(FVector2D(W,H));
}
void Fill(UCanvasPanel* Panel,UWidget* Widget)
{
	auto* Slot=Panel->AddChildToCanvas(Widget); Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0));
}
UTextBlock* Text(UWidgetBlueprint* BP,UCanvasPanel* Panel,FName Name,const TCHAR* Value,int32 Size,float X,float Y,float W,float H,FLinearColor Color=FLinearColor(.95f,.87f,.66f))
{
	auto* T=Widget<UTextBlock>(BP,Name); T->SetText(FText::FromString(Value));
	T->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto")),Size,TEXT("Regular")));
	T->SetColorAndOpacity(Color); T->SetJustification(ETextJustify::Center); T->SetVisibility(ESlateVisibility::HitTestInvisible);
	Position(Panel,T,X,Y,W,H); return T;
}
void ButtonStyle(UButton* Button,bool bCard)
{
	FButtonStyle Style;
	Style.SetNormal(FSlateColorBrush(bCard?FLinearColor(.025f,.033f,.047f,.94f):FLinearColor::Transparent));
	Style.SetHovered(FSlateColorBrush(bCard?FLinearColor(.035f,.075f,.085f,.97f):FLinearColor::Transparent));
	Style.SetPressed(FSlateColorBrush(bCard?FLinearColor(.025f,.1f,.1f,.97f):FLinearColor::Transparent));
	Style.SetDisabled(Style.Normal); Style.NormalPadding=FMargin(0); Style.PressedPadding=FMargin(0);
	Button->SetStyle(Style);
}
void BuildCard(UWidgetBlueprint* BP)
{
	if (!BP || BP->WidgetTree->RootWidget) return;
	auto* Root=Widget<UCanvasPanel>(BP,TEXT("CardRoot")); BP->WidgetTree->RootWidget=Root;
	auto* Button=Widget<UButton>(BP,TEXT("CardButton")); ButtonStyle(Button,true); Position(Root,Button,18,16,324,508);
	auto* Content=Widget<UCanvasPanel>(BP,TEXT("CardContent")); Button->AddChild(Content);
	auto* Portrait=Widget<UImage>(BP,TEXT("Portrait")); Portrait->SetVisibility(ESlateVisibility::HitTestInvisible); Position(Content,Portrait,0,20,324,360);
	Text(BP,Content,TEXT("NameText"),TEXT("职业"),34,0,365,324,50);
	Text(BP,Content,TEXT("RoleText"),TEXT("战斗定位"),20,0,420,324,32);
	Text(BP,Content,TEXT("DescriptionText"),TEXT("职业说明"),18,12,465,300,32);
	auto* Frame=Widget<UImage>(BP,TEXT("Frame")); Frame->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(Root,Frame);
	Text(BP,Root,TEXT("SelectionText"),TEXT(""),20,155,25,175,32,FLinearColor(.4f,1,.85f));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP); UEditorAssetLibrary::SaveLoadedAsset(BP,false);
}
void BuildScreen(UWidgetBlueprint* BP,UWidgetBlueprint* Card)
{
	if (!BP || !Card || BP->WidgetTree->RootWidget) return;
	CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject())->SetIsFocusable(true);
	auto* Root=Widget<UCanvasPanel>(BP,TEXT("ScreenRoot")); BP->WidgetTree->RootWidget=Root;
	auto* Background=Widget<UImage>(BP,TEXT("BackgroundImage")); Background->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(Root,Background);
	auto* Scale=Widget<UScaleBox>(BP,TEXT("ResponsiveLayout")); Scale->SetStretch(EStretch::ScaleToFit); Fill(Root,Scale);
	auto* Size=Widget<USizeBox>(BP,TEXT("DesignSize")); Size->SetWidthOverride(1600); Size->SetHeightOverride(900); Scale->AddChild(Size);
	auto* Canvas=Widget<UCanvasPanel>(BP,TEXT("Layout")); Size->AddChild(Canvas);
	Text(BP,Canvas,TEXT("GameTitle"),TEXT("BoldersDemo"),26,28,26,260,44);
	Text(BP,Canvas,TEXT("Title"),TEXT("选择你的职业"),52,400,35,800,78);
	Text(BP,Canvas,TEXT("Subtitle"),TEXT("每位玩家控制一名角色"),23,400,112,800,40);
	const FName Names[]={TEXT("WarriorCard"),TEXT("ArcherCard"),TEXT("MageCard")};
	for (int32 I=0;I<3;++I)
	{
		auto* C=BP->WidgetTree->ConstructWidget<UClassCardWidget>(Card->GeneratedClass.Get(),Names[I]); C->bIsVariable=true;
		Position(Canvas,C,200+420*I,170,360,540);
	}
	Text(BP,Canvas,TEXT("StatusText"),TEXT("请选择一个职业"),25,400,737,800,42);
	auto* Plate=Widget<UImage>(BP,TEXT("ConfirmImage")); Plate->SetVisibility(ESlateVisibility::HitTestInvisible); Position(Canvas,Plate,540,744,520,174);
	auto* Confirm=Widget<UButton>(BP,TEXT("ConfirmButton")); ButtonStyle(Confirm,false); Position(Canvas,Confirm,550,787,500,86);
	Text(BP,Canvas,TEXT("ConfirmLabel"),TEXT("确认职业"),34,550,803,500,55,FLinearColor(.1f,.055f,.015f));
	auto* Clear=Widget<UButton>(BP,TEXT("ClearButton")); ButtonStyle(Clear,true); Position(Canvas,Clear,45,815,180,50);
	Text(BP,Canvas,TEXT("ClearLabel"),TEXT("取消选择"),21,45,826,180,40);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP); UEditorAssetLibrary::SaveLoadedAsset(BP,false);
}
void Build()
{
	for (const TCHAR* Role:{TEXT("Warrior"),TEXT("Archer"),TEXT("Mage")})
		BuildAnimation(LoadObject<UClassSelectionData>(nullptr,*(FString(TEXT("/Game/BoldersDemo/Assets/UI/ClassSelection/DA_Class_"))+Role)));
	auto* Card=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/BoldersDemo/Blueprints/UI/ClassSelection/WBP_ClassCard"));
	auto* Screen=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/BoldersDemo/Blueprints/UI/ClassSelection/WBP_ClassSelection"));
	BuildCard(Card); BuildScreen(Screen,Card);
}
}
static FAutoConsoleCommand BuildClassSelection(TEXT("Bolders.BuildClassSelection"),TEXT("Build class preview animation graphs and selection widget layouts."),FConsoleCommandDelegate::CreateStatic(&ClassSelectionSetup::Build));
#endif
