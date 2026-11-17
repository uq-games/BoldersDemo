#if WITH_EDITOR
#include "Characters/BaseCharacterAnimInstance.h"
#include "Combat/AnimNotify_AttackHit.h"
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

namespace RangedAnimationSetup
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
void BuildCharacter(const FString& Role, const FString& WalkName, const FString& RunName)
{
	const FString Source = TEXT("/Game/BoldersDemo/Assets/Characters/") + Role;
	auto* Idle = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("/Animations/Idle/A_") + Role + TEXT("_GeneralIdle_A")));
	auto* Walk = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("/Animations/Locomotion/") + WalkName));
	auto* Run = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("/Animations/Locomotion/") + RunName));
	auto* Hit = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("/Animations/HitReactions/A_") + Role + TEXT("_GeneralHit_A")));
	auto* Death = LoadObject<UAnimSequence>(nullptr, *(Source + TEXT("/Animations/Death/A_") + Role + TEXT("_GeneralDeath_A")));
	auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *(Source + TEXT("/SK_") + Role));
	if (!Idle || !Walk || !Run || !Hit || !Death || !Mesh) { UE_LOG(LogTemp, Error, TEXT("RangedAnimation: missing %s assets"), *Role); return; }
	if (Idle->GetSkeleton() != Mesh->GetSkeleton() || Hit->GetSkeleton() != Mesh->GetSkeleton() || Death->GetSkeleton() != Mesh->GetSkeleton()) return;
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString Folder = Source + TEXT("/Animations");
	auto* BS = LoadObject<UBlendSpace1D>(nullptr, *(Folder + TEXT("/BS_") + Role + TEXT("_Locomotion")));
	if (!BS)
	{
		auto* Factory = NewObject<UBlendSpaceFactory1D>(); Factory->TargetSkeleton = Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh = Mesh;
		BS = CastChecked<UBlendSpace1D>(Tools.CreateAsset(TEXT("BS_") + Role + TEXT("_Locomotion"), Folder, UBlendSpace1D::StaticClass(), Factory));
		auto* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
		auto* Axis = Property->ContainerPtrToValuePtr<FBlendParameter>(BS);
		Axis->DisplayName = TEXT("Speed"); Axis->Min = 0; Axis->Max = 300; Axis->GridNum = 6;
		BS->AddSample(Idle, FVector::ZeroVector); BS->AddSample(Walk, FVector(150,0,0)); BS->AddSample(Run, FVector(300,0,0));
		BS->ValidateSampleData(); BS->ResampleData(); BS->PostEditChange();
	}
	auto* Montage = LoadObject<UAnimMontage>(nullptr, *(Folder + TEXT("/AM_") + Role + TEXT("_Hit")));
	if (!Montage)
	{
		auto* Factory = NewObject<UAnimMontageFactory>(); Factory->TargetSkeleton = Mesh->GetSkeleton();
		Montage = CastChecked<UAnimMontage>(Tools.CreateAsset(TEXT("AM_") + Role + TEXT("_Hit"), Folder, UAnimMontage::StaticClass(), Factory));
		Montage->SlotAnimTracks.Empty(); Montage->CompositeSections.Empty();
		FSlotAnimationTrack Track; Track.SlotName = TEXT("DefaultSlot");
		FAnimSegment Segment; Segment.SetAnimReference(Hit, true); Segment.AnimEndTime = Hit->GetPlayLength(); Segment.LoopingCount = 1;
		Track.AnimTrack.AnimSegments.Add(Segment); Montage->SlotAnimTracks.Add(Track);
		Montage->SetCompositeLength(Hit->GetPlayLength()); Montage->AddAnimCompositeSection(TEXT("Default"), 0);
		Montage->BlendIn.SetBlendTime(.08f); Montage->BlendOut.SetBlendTime(.12f); Montage->PostEditChange();
	}
	const FString BPFolder = TEXT("/Game/BoldersDemo/Blueprints/Characters/") + Role;
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, *(BPFolder + TEXT("/ABP_") + Role));
	if (!BP)
	{
		auto* Factory = NewObject<UAnimBlueprintFactory>(); Factory->ParentClass = UBaseCharacterAnimInstance::StaticClass();
		Factory->TargetSkeleton = Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh = Mesh;
		BP = CastChecked<UAnimBlueprint>(Tools.CreateAsset(TEXT("ABP_") + Role, BPFolder, UAnimBlueprint::StaticClass(), Factory));
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
	UE_LOG(LogTemp, Log, TEXT("RangedAnimation: %s status=%d hit=%.2f death=%.2f"), *Role, int32(BP->Status), Hit->GetPlayLength(), Death->GetPlayLength());
}
}

namespace RangedAnimationSetup
{
void BuildAttack(const FString& Role, const TArray<FString>& SequenceNames, int32 ReleaseSegment, float ReleaseFraction)
{
	const FString Folder = TEXT("/Game/BoldersDemo/Assets/Characters/") + Role + TEXT("/Animations");
	TArray<UAnimSequence*> Sequences;
	for (const FString& Name : SequenceNames)
	{
		auto* Sequence = LoadObject<UAnimSequence>(nullptr, *(Folder + TEXT("/Combat/") + Name));
		if (!Sequence) { UE_LOG(LogTemp, Error, TEXT("Missing ranged animation: %s"), *Name); return; }
		Sequences.Add(Sequence);
	}
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const FString Name = TEXT("AM_") + Role + TEXT("_BasicAttack");
	auto* Montage = LoadObject<UAnimMontage>(nullptr, *(Folder + TEXT("/Montages/") + Name));
	if (Montage) return; // Preserve subsequent artist timing edits.
	auto* Factory = NewObject<UAnimMontageFactory>(); Factory->TargetSkeleton = Sequences[0]->GetSkeleton();
	Montage = CastChecked<UAnimMontage>(Tools.CreateAsset(Name, Folder + TEXT("/Montages"), UAnimMontage::StaticClass(), Factory));
	Montage->SlotAnimTracks.Empty(); Montage->CompositeSections.Empty();
	FSlotAnimationTrack Track; Track.SlotName = TEXT("DefaultSlot");
	float Length = 0.f, ReleaseTime = 0.f;
	for (int32 I = 0; I < Sequences.Num(); ++I)
	{
		FAnimSegment Segment; Segment.SetAnimReference(Sequences[I], true);
		Segment.StartPos = Length; Segment.AnimEndTime = Sequences[I]->GetPlayLength(); Segment.LoopingCount = 1;
		if (I == ReleaseSegment) ReleaseTime = Length + Segment.AnimEndTime * ReleaseFraction;
		Length += Segment.AnimEndTime; Track.AnimTrack.AnimSegments.Add(Segment);
	}
	Montage->SlotAnimTracks.Add(Track); Montage->SetCompositeLength(Length); Montage->AddAnimCompositeSection(TEXT("Default"), 0);
	FAnimNotifyEvent Event; Event.Notify = NewObject<UAnimNotify_AttackHit>(Montage);
	Event.NotifyName = TEXT("AttackHit"); Event.Link(Montage, ReleaseTime); Montage->Notifies.Add(Event);
	Montage->BlendIn.SetBlendTime(.08f); Montage->BlendOut.SetBlendTime(.15f); Montage->PostEditChange();
	UEditorAssetLibrary::SaveLoadedAsset(Montage, false);
	UE_LOG(LogTemp, Log, TEXT("RangedAnimation: %s duration=%.3f release=%.3f"), *Name, Length, ReleaseTime);
}
void Build()
{
	BuildCharacter(TEXT("Archer"), TEXT("A_Archer_MovementBasic_Walking_A"), TEXT("A_Archer_MovementAdvanced_Running_HoldingBow"));
	BuildCharacter(TEXT("Mage"), TEXT("A_Mage_MovementBasic_Walking_A"), TEXT("A_Mage_MovementBasic_Running_A"));
	BuildAttack(TEXT("Archer"), {TEXT("Bow/A_Archer_CombatRanged_Ranged_Bow_Draw"), TEXT("Bow/A_Archer_CombatRanged_Ranged_Bow_Release")}, 1, .1f);
	BuildAttack(TEXT("Mage"), {TEXT("Magic/A_Mage_CombatRanged_Ranged_Magic_Shoot")}, 0, .4f);
}
}
static FAutoConsoleCommand BuildRangedAnimation(TEXT("Bolders.BuildRangedAnimation"),
	TEXT("Create archer and mage locomotion, death, hit and attack assets."), FConsoleCommandDelegate::CreateStatic(&RangedAnimationSetup::Build));
#endif
