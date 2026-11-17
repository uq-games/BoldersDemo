#if WITH_EDITOR
#include "Combat/AnimNotify_AttackHit.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimMontageFactory.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

static void BuildCombatAssets()
{
	const FString Root = TEXT("/Game/BoldersDemo/Assets/Characters/Warrior/Animations");
	auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const TCHAR* Names[] = { TEXT("Chop"), TEXT("Slice_Horizontal"), TEXT("Stab") };
	// Contact moments sampled independently from each source motion.
	const float HitFractions[] = { 0.56f, 0.18f, 0.30f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Name = Names[Index];
		auto* Sequence = LoadObject<UAnimSequence>(nullptr, *(Root + TEXT("/Combat/OneHanded/A_Warrior_CombatMelee_Melee_1H_Attack_") + Name));
		if (!Sequence) { UE_LOG(LogTemp, Error, TEXT("CombatSetup: missing %s"), *Name); return; }
		const FString Folder = Root + TEXT("/Montages");
		const FString AssetName = TEXT("AM_Warrior_") + Name;
		auto* Montage = LoadObject<UAnimMontage>(nullptr, *(Folder + TEXT("/") + AssetName));
		if (!Montage)
		{
			auto* Factory = NewObject<UAnimMontageFactory>();
			Factory->TargetSkeleton = Sequence->GetSkeleton();
			Montage = CastChecked<UAnimMontage>(Tools.CreateAsset(AssetName, Folder, UAnimMontage::StaticClass(), Factory));
		}
		Montage->Modify();
		Montage->SlotAnimTracks.Empty();
		FSlotAnimationTrack Track;
		Track.SlotName = TEXT("DefaultSlot");
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequence, true);
		Segment.AnimStartTime = 0;
		Segment.AnimEndTime = Sequence->GetPlayLength();
		Segment.AnimPlayRate = 1;
		Segment.LoopingCount = 1;
		Track.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Add(Track);
		Montage->SetCompositeLength(Sequence->GetPlayLength());
		Montage->CompositeSections.Empty();
		Montage->AddAnimCompositeSection(TEXT("Default"), 0.f);
		Montage->Notifies.Empty();
		FAnimNotifyEvent Event;
		Event.Notify = NewObject<UAnimNotify_AttackHit>(Montage);
		Event.NotifyName = TEXT("AttackHit");
		Event.Link(Montage, Sequence->GetPlayLength() * HitFractions[Index]);
		Montage->Notifies.Add(Event);
		Montage->PostEditChange();
		UEditorAssetLibrary::SaveLoadedAsset(Montage, false);
		UE_LOG(LogTemp, Log, TEXT("CombatSetup: montage %s length=%.3f"), *Montage->GetPathName(), Montage->GetPlayLength());
	}
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/BoldersDemo/Blueprints/Characters/Warrior/ABP_Warrior"));
	if (!BP) return;
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetFName() != TEXT("AnimGraph")) continue;
		UAnimGraphNode_Root* RootNode = nullptr;
		bool bHasSlot = false;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (auto* OutputNode = Cast<UAnimGraphNode_Root>(Node)) RootNode = OutputNode;
			if (Cast<UAnimGraphNode_Slot>(Node)) bHasSlot = true;
		}
		if (!RootNode || bHasSlot) continue;
		auto* Result = RootNode->FindPin(TEXT("Result"));
		if (!Result || Result->LinkedTo.IsEmpty()) continue;
		auto* Previous = Result->LinkedTo[0];
		auto* Slot = NewObject<UAnimGraphNode_Slot>(Graph);
		Slot->Node.SlotName = TEXT("DefaultSlot");
		Graph->AddNode(Slot, false, false);
		Slot->CreateNewGuid(); Slot->PostPlacedNewNode(); Slot->AllocateDefaultPins();
		Slot->NodePosX = RootNode->NodePosX - 180;
		Result->BreakAllPinLinks();
		Graph->GetSchema()->TryCreateConnection(Previous, Slot->FindPin(TEXT("Source")));
		Graph->GetSchema()->TryCreateConnection(Slot->FindPin(TEXT("Pose")), Result);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	UEditorAssetLibrary::SaveLoadedAsset(BP, false);
	UE_LOG(LogTemp, Log, TEXT("CombatSetup: ABP status=%d"), (int32)BP->Status);
}

static FAutoConsoleCommand BuildCombatAssetsCommand(TEXT("Bolders.BuildCombatAssets"),
	TEXT("Build warrior attack montages and connect animation slot."), FConsoleCommandDelegate::CreateStatic(&BuildCombatAssets));
#endif
