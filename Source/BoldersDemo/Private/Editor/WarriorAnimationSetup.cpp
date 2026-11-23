// Editor-only, repeatable asset setup. Never runs in the game.
#if WITH_EDITOR
#include "Characters/WarriorAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

static void BuildWarriorAnimation()
{
	const FString Root = TEXT("/Game/BoldersDemo/Assets/Characters/Warrior");
	auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *(Root + TEXT("/SK_Warrior")));
	if (!Mesh) return;
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString BSPath = Root + TEXT("/Animations/BS_Warrior_Locomotion");
	auto* BS = LoadObject<UBlendSpace1D>(nullptr, *BSPath);
	if (!BS)
	{
		auto* Factory = NewObject<UBlendSpaceFactory1D>();
		Factory->TargetSkeleton = Mesh->GetSkeleton();
		Factory->PreviewSkeletalMesh = Mesh;
		BS = Cast<UBlendSpace1D>(Tools.CreateAsset(TEXT("BS_Warrior_Locomotion"), Root + TEXT("/Animations"), UBlendSpace1D::StaticClass(), Factory));
		if (!BS) return;
		auto* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
		FBlendParameter* Axis = Property->ContainerPtrToValuePtr<FBlendParameter>(BS);
		Axis->DisplayName = TEXT("Speed");
		Axis->Min = 0.f; Axis->Max = 300.f; Axis->GridNum = 6;
		const TCHAR* Paths[] = { TEXT("/Animations/General/A_Warrior_GeneralIdle_A"), TEXT("/Animations/MovementBasic/A_Warrior_MovementBasic_Walking_A"), TEXT("/Animations/MovementBasic/A_Warrior_MovementBasic_Running_A") };
		for (int32 I = 0; I < 3; ++I)
		{
			auto* Anim = LoadObject<UAnimSequence>(nullptr, *(Root + Paths[I]));
			if (!Anim) { UE_LOG(LogTemp, Error, TEXT("Missing warrior animation")); return; }
			BS->AddSample(Anim, FVector(I * 150.f, 0, 0));
		}
		BS->ValidateSampleData();
		BS->ResampleData();
		BS->PostEditChange();
	}
	const FString ABPFolder = TEXT("/Game/BoldersDemo/Blueprints/Characters/Warrior");
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, *(ABPFolder + TEXT("/ABP_Warrior")));
	if (!BP)
	{
		auto* Factory = NewObject<UAnimBlueprintFactory>();
		Factory->ParentClass = UWarriorAnimInstance::StaticClass();
		Factory->TargetSkeleton = Mesh->GetSkeleton();
		Factory->PreviewSkeletalMesh = Mesh;
		BP = Cast<UAnimBlueprint>(Tools.CreateAsset(TEXT("ABP_Warrior"), ABPFolder, UAnimBlueprint::StaticClass(), Factory));
		if (!BP) return;
		TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph->GetFName() != TEXT("AnimGraph")) continue;
			UAnimGraphNode_Root* RootNode = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes) if (auto* R = Cast<UAnimGraphNode_Root>(Node)) RootNode = R;
			if (!RootNode) continue;
			auto* Player = NewObject<UAnimGraphNode_BlendSpacePlayer>(Graph);
			Player->SetAnimationAsset(BS);
			Graph->AddNode(Player, false, false);
			Player->CreateNewGuid(); Player->PostPlacedNewNode(); Player->AllocateDefaultPins();
			Player->NodePosX = -300;
			auto* Speed = NewObject<UK2Node_VariableGet>(Graph);
			Speed->VariableReference.SetSelfMember(TEXT("Speed"));
			Graph->AddNode(Speed, false, false);
			Speed->CreateNewGuid(); Speed->PostPlacedNewNode(); Speed->AllocateDefaultPins();
			Speed->NodePosX = -600;
			auto* XPin = Player->FindPin(TEXT("X"));
			auto* Pose = Player->FindPin(TEXT("Pose"));
			auto* Result = RootNode->FindPin(TEXT("Result"));
			if (!XPin || !Pose || !Result || !Graph->GetSchema()->TryCreateConnection(Speed->GetValuePin(), XPin)
				|| !Graph->GetSchema()->TryCreateConnection(Pose, Result))
			{ UE_LOG(LogTemp, Error, TEXT("Warrior animation graph wiring failed")); return; }
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	// Also repair/recompile an existing generated asset when rerunning setup.
	TArray<UEdGraph*> ExistingGraphs; BP->GetAllGraphs(ExistingGraphs);
	for (UEdGraph* Graph : ExistingGraphs)
		for (UEdGraphNode* Node : Graph->Nodes)
			if (auto* Variable = Cast<UK2Node_VariableGet>(Node))
				if (Variable->VariableReference.GetMemberName() == TEXT("Speed"))
				{
					Variable->VariableReference.SetSelfMember(TEXT("Speed"));
					Variable->ReconstructNode();
				}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	UEditorAssetLibrary::SaveLoadedAsset(BS);
	UEditorAssetLibrary::SaveLoadedAsset(BP, false);
	UE_LOG(LogTemp, Log, TEXT("WarriorAnimation: Built blend space and animation blueprint; status=%d"), (int32)BP->Status);
}
static FAutoConsoleCommand BuildWarriorAnimationCommand(TEXT("Bolders.BuildWarriorAnimation"),
	TEXT("Create the demo warrior blend space and animation blueprint."), FConsoleCommandDelegate::CreateStatic(&BuildWarriorAnimation));

#endif
