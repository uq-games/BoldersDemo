#if WITH_EDITOR
#include "Characters/Animation/BaseCharacterAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

namespace MushroomAnimationSetup
{
template<class T> T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
	auto* Node = NewObject<T>(Graph);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid(); Node->PostPlacedNewNode(); Node->AllocateDefaultPins();
	Node->NodePosX = X; Node->NodePosY = Y;
	return Node;
}
void Connect(UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* To)
{
	check(From && To);
	check(Graph->GetSchema()->TryCreateConnection(From, To));
}
UK2Node_VariableGet* Variable(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
	auto* Node = NewObject<UK2Node_VariableGet>(Graph);
	Node->VariableReference.SetSelfMember(Name);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid(); Node->PostPlacedNewNode(); Node->AllocateDefaultPins();
	Node->NodePosX = X; Node->NodePosY = Y;
	return Node;
}
void Build()
{
	const FString Source = TEXT("/Game/MonsterForSurvivalGame/Animation/Polyart/Mushroom/SmileVersion/Mushroom_");
	auto* Idle = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("IdleNormalSmile_ANIM")));
	auto* Walk = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("walkFWDSmile_ANIM")));
	auto* Hit = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("GetHitSmile_ANIM")));
	auto* Death = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("DieSmile_ANIM")));
	auto* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/MonsterForSurvivalGame/Mesh/Polyart/Mushroom_SK"));
	if (!Idle || !Walk || !Hit || !Death || !Mesh) { UE_LOG(LogTemp, Error, TEXT("MushroomAnimation: missing source assets")); return; }
	if (Idle->GetSkeleton() != Mesh->GetSkeleton() || Hit->GetSkeleton() != Mesh->GetSkeleton() || Death->GetSkeleton() != Mesh->GetSkeleton()) return;
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString Folder = TEXT("/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Animations");
	auto* BS = LoadObject<UBlendSpace1D>(nullptr, *(Folder + TEXT("/BS_Mushroom_Locomotion")));
	if (!BS)
	{
		auto* Factory = NewObject<UBlendSpaceFactory1D>(); Factory->TargetSkeleton = Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh = Mesh;
		BS = CastChecked<UBlendSpace1D>(Tools.CreateAsset(TEXT("BS_Mushroom_Locomotion"), Folder, UBlendSpace1D::StaticClass(), Factory));
		auto* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
		auto* Axis = Property->ContainerPtrToValuePtr<FBlendParameter>(BS);
		Axis->DisplayName = TEXT("Speed"); Axis->Min = 0; Axis->Max = 300; Axis->GridNum = 6;
		BS->AddSample(Idle, FVector::ZeroVector); BS->AddSample(Walk, FVector(300,0,0));
		BS->ValidateSampleData(); BS->ResampleData(); BS->PostEditChange();
	}
	auto* Montage = LoadObject<UAnimMontage>(nullptr, *(Folder + TEXT("/AM_Mushroom_Hit")));
	if (!Montage)
	{
		auto* Factory = NewObject<UAnimMontageFactory>(); Factory->TargetSkeleton = Mesh->GetSkeleton();
		Montage = CastChecked<UAnimMontage>(Tools.CreateAsset(TEXT("AM_Mushroom_Hit"), Folder, UAnimMontage::StaticClass(), Factory));
		Montage->SlotAnimTracks.Empty(); Montage->CompositeSections.Empty();
		FSlotAnimationTrack Track; Track.SlotName = TEXT("DefaultSlot");
		FAnimSegment Segment; Segment.SetAnimReference(Hit, true); Segment.AnimEndTime = Hit->GetPlayLength(); Segment.LoopingCount = 1;
		Track.AnimTrack.AnimSegments.Add(Segment); Montage->SlotAnimTracks.Add(Track);
		Montage->SetCompositeLength(Hit->GetPlayLength()); Montage->AddAnimCompositeSection(TEXT("Default"), 0);
		Montage->BlendIn.SetBlendTime(.08f); Montage->BlendOut.SetBlendTime(.12f); Montage->PostEditChange();
	}
	const FString BPFolder = TEXT("/Game/BoldersDemo/Blueprints/Characters/Enemy");
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, *(BPFolder + TEXT("/ABP_EnemyMushroom")));
	if (!BP)
	{
		auto* Factory = NewObject<UAnimBlueprintFactory>(); Factory->ParentClass = UBaseCharacterAnimInstance::StaticClass();
		Factory->TargetSkeleton = Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh = Mesh;
		BP = CastChecked<UAnimBlueprint>(Tools.CreateAsset(TEXT("ABP_EnemyMushroom"), BPFolder, UAnimBlueprint::StaticClass(), Factory));
		TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph->GetFName() != TEXT("AnimGraph")) continue;
			UAnimGraphNode_Root* Root = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes) if (auto* R = Cast<UAnimGraphNode_Root>(Node)) Root = R;
			if (!Root) continue;
			auto* Locomotion = AddNode<UAnimGraphNode_BlendSpacePlayer>(Graph, -600, 220);
			Locomotion->SetAnimationAsset(BS); Locomotion->ReconstructNode();
			auto* Speed = Variable(Graph, TEXT("Speed"), -900, 220);
			Connect(Graph, Speed->GetValuePin(), Locomotion->FindPin(TEXT("X")));
			auto* Slot = AddNode<UAnimGraphNode_Slot>(Graph, -350, 220); Slot->Node.SlotName = TEXT("DefaultSlot");
			Connect(Graph, Locomotion->FindPin(TEXT("Pose")), Slot->FindPin(TEXT("Source")));
			auto* Dying = AddNode<UAnimGraphNode_SequencePlayer>(Graph, -350, -100);
			Dying->Node.SetSequence(Death); Dying->Node.SetLoopAnimation(false);
			auto* Blend = AddNode<UAnimGraphNode_BlendListByBool>(Graph, -80, 0);
			auto* Dead = Variable(Graph, TEXT("bDead"), -350, -240);
			Connect(Graph, Dead->GetValuePin(), Blend->FindPin(TEXT("bActiveValue")));
			Connect(Graph, Dying->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("BlendPose_0")));
			Connect(Graph, Slot->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("BlendPose_1")));
			Root->NodePosX = 250;
			Connect(Graph, Blend->FindPin(TEXT("Pose")), Root->FindPin(TEXT("Result")));
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	UEditorAssetLibrary::SaveLoadedAsset(BS, false); UEditorAssetLibrary::SaveLoadedAsset(Montage, false); UEditorAssetLibrary::SaveLoadedAsset(BP, false);
	UE_LOG(LogTemp, Log, TEXT("MushroomAnimation: status=%d hit=%.2f death=%.2f"), int32(BP->Status), Hit->GetPlayLength(), Death->GetPlayLength());
}
}
static FAutoConsoleCommand BuildMushroomAnimation(TEXT("Bolders.BuildMushroomAnimation"),
	TEXT("Create mushroom locomotion, hit montage and non-looping death graph."), FConsoleCommandDelegate::CreateStatic(&MushroomAnimationSetup::Build));
#endif
