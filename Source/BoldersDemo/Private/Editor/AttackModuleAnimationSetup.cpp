#if WITH_EDITOR

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AssetToolsModule.h"
#include "Combat/AnimNotify_AttackHit.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimMontageFactory.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace
{
	template<typename T>
	T* AddNode(UEdGraph& Graph, int32 X, int32 Y)
	{
		auto* Node = NewObject<T>(&Graph);
		Graph.AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		return Node;
	}

	void AddGreatswordIdle()
	{
		auto* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
			TEXT("/Game/BoldersDemo/Blueprints/Characters/Warrior/ABP_Warrior"));
		auto* Idle = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/BoldersDemo/Assets/Characters/Warrior/Animations/Combat/TwoHanded/A_Warrior_2H_Idle"));
		if (!Blueprint || !Idle) return;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph->GetFName() != TEXT("AnimGraph")) continue;
			UAnimGraphNode_Slot* Slot = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (const auto* Variable = Cast<UK2Node_VariableGet>(Node);
					Variable && Variable->VariableReference.GetMemberName() == TEXT("bUseGreatswordIdle")) return;
				if (auto* Candidate = Cast<UAnimGraphNode_Slot>(Node); Candidate && Candidate->Node.SlotName == TEXT("DefaultSlot"))
					Slot = Candidate;
			}
			auto* Input = Slot ? Slot->FindPin(TEXT("Source")) : nullptr;
			if (!Input || Input->LinkedTo.Num() != 1) return;
			UEdGraphPin* LocomotionPose = Input->LinkedTo[0];
			auto* IdlePlayer = AddNode<UAnimGraphNode_SequencePlayer>(*Graph, Slot->NodePosX - 475, Slot->NodePosY - 215);
			IdlePlayer->Node.SetSequence(Idle);
			IdlePlayer->Node.SetLoopAnimation(true);
			auto* Blend = AddNode<UAnimGraphNode_BlendListByBool>(*Graph, Slot->NodePosX - 230, Slot->NodePosY);
			auto* Variable = NewObject<UK2Node_VariableGet>(Graph);
			Variable->VariableReference.SetSelfMember(TEXT("bUseGreatswordIdle"));
			Graph->AddNode(Variable, false, false);
			Variable->CreateNewGuid();
			Variable->PostPlacedNewNode();
			Variable->AllocateDefaultPins();
			Variable->NodePosX = Slot->NodePosX - 475;
			Variable->NodePosY = Slot->NodePosY + 230;
			Input->BreakAllPinLinks();
			const auto* Schema = Graph->GetSchema();
			const bool bConnected = Schema->TryCreateConnection(LocomotionPose, Blend->FindPin(TEXT("BlendPose_1")))
				&& Schema->TryCreateConnection(IdlePlayer->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("BlendPose_0")))
				&& Schema->TryCreateConnection(Variable->GetValuePin(), Blend->FindPin(TEXT("bActiveValue")))
				&& Schema->TryCreateConnection(Blend->FindPin(TEXT("Pose")), Input);
			if (!bConnected)
			{
				Graph->RemoveNode(Blend);
				Graph->RemoveNode(IdlePlayer);
				Graph->RemoveNode(Variable);
				Schema->TryCreateConnection(LocomotionPose, Input);
				UE_LOG(LogTemp, Error, TEXT("Greatsword idle: failed to wire the warrior animation graph"));
				return;
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
		}
	}

	void BuildAttackModuleMontages()
	{
		struct FAnimation
		{
			const TCHAR* Role;
			const TCHAR* Name;
			const TCHAR* Sequence;
			float Contact;
		};
		const FAnimation Animations[] = {
			{TEXT("Warrior"), TEXT("GreatswordChop"), TEXT("TwoHanded/A_Warrior_2H_Attack_Chop"), .51f},
			{TEXT("Warrior"), TEXT("GreatswordSweep"), TEXT("TwoHanded/A_Warrior_2H_Attack_Slice"), .39f},
			{TEXT("Mage"), TEXT("WaterWave"), TEXT("Magic/A_Mage_CombatRanged_Ranged_Magic_Spellcasting"), .55f},
			{TEXT("Mage"), TEXT("SkyStrike"), TEXT("Magic/A_Mage_CombatRanged_Ranged_Magic_Raise"), .47f},
			{TEXT("Mage"), TEXT("IceLance"), TEXT("Magic/A_Mage_CombatRanged_Ranged_Magic_Spellcasting_Long"), .63f}
		};
		auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (const FAnimation& Data : Animations)
		{
			const FString Root = FString::Printf(TEXT("/Game/BoldersDemo/Assets/Characters/%s/Animations"), Data.Role);
			const FString AssetName = FString::Printf(TEXT("AM_%s_%s"), Data.Role, Data.Name);
			const FString Folder = Root / TEXT("Montages");
			if (UEditorAssetLibrary::DoesAssetExist(Folder / AssetName)) continue;
			auto* Sequence = LoadObject<UAnimSequence>(nullptr, *(Root / TEXT("Combat") / Data.Sequence));
			if (!Sequence)
			{
				UE_LOG(LogTemp, Error, TEXT("AttackModuleAnimation: missing source %s"), Data.Sequence);
				continue;
			}
			auto* Factory = NewObject<UAnimMontageFactory>();
			Factory->TargetSkeleton = Sequence->GetSkeleton();
			auto* Montage = Cast<UAnimMontage>(Tools.CreateAsset(AssetName, Folder, UAnimMontage::StaticClass(), Factory));
			if (!Montage) continue;
			FSlotAnimationTrack Track;
			Track.SlotName = TEXT("DefaultSlot");
			FAnimSegment Segment;
			Segment.SetAnimReference(Sequence, true);
			Segment.AnimEndTime = Sequence->GetPlayLength();
			Track.AnimTrack.AnimSegments.Add(Segment);
			Montage->SlotAnimTracks = {Track};
			Montage->CompositeSections.Reset();
			Montage->SetCompositeLength(Segment.AnimEndTime);
			Montage->AddAnimCompositeSection(TEXT("Default"), 0.f);
			FAnimNotifyEvent Event;
			Event.Notify = NewObject<UAnimNotify_AttackHit>(Montage);
			Event.NotifyName = TEXT("AttackHit");
			Event.Link(Montage, Segment.AnimEndTime * Data.Contact);
			Montage->Notifies = {Event};
			Montage->BlendIn.SetBlendTime(.09f);
			Montage->BlendOut.SetBlendTime(.13f);
			Montage->PostEditChange();
			UEditorAssetLibrary::SaveLoadedAsset(Montage, false);
		}
		AddGreatswordIdle();
	}

	FAutoConsoleCommand BuildAttackModuleMontagesCommand(TEXT("Bolders.BuildAttackModuleMontages"),
		TEXT("Create any missing weapon and spell attack montages."),
		FConsoleCommandDelegate::CreateStatic(&BuildAttackModuleMontages));
}
#endif
