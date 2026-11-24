#if WITH_EDITOR

#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"

#include <initializer_list>

namespace
{
	struct FLayer
	{
		const TCHAR* Name;
		const TCHAR* Template;
		const TCHAR* Material;
		float Rate;
		float Life;
		float Size;
		float Speed;
		bool bStreak = false;
		bool bPlane = false;
		float Radius = 8.f;
		float Gravity = -160.f;
		float Cone = 32.f;
	};

	const FString AssetRoot = TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Effects/");

	void TuneEmitter(FVersionedNiagaraEmitterData& Data, const FLayer& Layer, FString& Audit)
	{
		TArray<UNiagaraScript*> Scripts;
		Data.GetScripts(Scripts, false, true);
		for (UNiagaraScript* Script : Scripts)
		{
			TArray<FNiagaraVariable> Parameters;
			auto& Store = Script->RapidIterationParameters;
			Store.GetParameters(Parameters);
			for (const FNiagaraVariable& Param : Parameters)
			{
				const FString Name = Param.GetName().ToString();
				FString Key = Name;
				Key.ReplaceInline(TEXT(" "), TEXT(""));
				Key.ToLowerInline();
				const auto& Type = Param.GetType();
				const bool bSize = Key.Contains(TEXT("spritesize"));
				if (Type == FNiagaraTypeDefinition::GetFloatDef())
				{
					const float Old = Store.GetParameterValue<float>(Param);
					float New = Old;
					if (Key.EndsWith(TEXT(".spawnrate"))) New = Layer.Rate;
					else if (Key.Contains(TEXT("initializ")) && Key.Contains(TEXT("lifetime")) && !Key.Contains(TEXT("mode")))
						New = Key.Contains(TEXT("min")) ? Layer.Life * .72f : Layer.Life;
					else if (Key.EndsWith(TEXT(".randomrangefloat.minimum"))) New = Layer.Speed * .58f;
					else if (Key.EndsWith(TEXT(".randomrangefloat.maximum"))) New = Layer.Speed;
					else if (Key.EndsWith(TEXT(".shapelocation.sphereradius"))) New = Layer.Radius;
					else if (Key.EndsWith(TEXT(".addvelocity.coneangle"))) New = Layer.Cone;
					else if (bSize && Key.Contains(TEXT("initializ")) && !Key.Contains(TEXT("scale")))
						New = Key.Contains(TEXT("min")) ? Layer.Size * .56f : Layer.Size;
					if (New != Old) Store.SetParameterValue(New, Param);
					Audit += FString::Printf(TEXT("%s\t%.3f\t%.3f\n"), *Name, Old, New);
				}
				else if (Type == FNiagaraTypeDefinition::GetIntDef() && Key.EndsWith(TEXT(".spawncount")))
				{
					const int32 Old = Store.GetParameterValue<int32>(Param);
					const int32 Count = FMath::RoundToInt32(Layer.Rate);
					Store.SetParameterValue(Count, Param);
					Audit += FString::Printf(TEXT("%s\t%d\t%d\n"), *Name, Old, Count);
				}
				else if (Type == FNiagaraTypeDefinition::GetVec2Def() && bSize
					&& Key.Contains(TEXT("initializ")) && !Key.Contains(TEXT("scale")))
				{
					const float Size = Key.Contains(TEXT("min")) ? Layer.Size * .56f : Layer.Size;
					Store.SetParameterValue(FVector2f(Size, Size), Param);
					Audit += FString::Printf(TEXT("%s\tvector2\t%.2f\n"), *Name, Size);
				}
				else if (Type == FNiagaraTypeDefinition::GetVec3Def() && Key.EndsWith(TEXT(".gravityforce.gravity")))
				{
					Store.SetParameterValue(FVector3f(0.f, 0.f, Layer.Gravity), Param);
					Audit += FString::Printf(TEXT("%s\tgravity Z\t%.2f\n"), *Name, Layer.Gravity);
				}
				else
				{
					Audit += FString::Printf(TEXT("%s\t%s\n"), *Name, *Type.GetNameText().ToString());
				}
			}
		}
	}

	bool BuildSystem(const TCHAR* Suffix, std::initializer_list<FLayer> Layers, FString& Audit)
	{
		auto* System = LoadObject<UNiagaraSystem>(nullptr, *(AssetRoot + TEXT("NS_Foundry") + Suffix));
		if (!System) return false;
		System->Modify();
		TSet<FGuid> Old;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Old.Add(Handle.GetId());
		if (!Old.IsEmpty()) System->RemoveEmitterHandlesById(Old);
		for (const auto& Layer : Layers)
		{
			const FString Template = FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/")) + Layer.Template;
			auto* Source = LoadObject<UNiagaraEmitter>(nullptr, *Template);
			auto* Material = LoadObject<UMaterialInterface>(nullptr,
				*(AssetRoot + TEXT("M_FoundryParticle_") + Layer.Material));
			if (!Source || !Material) return false;
			const FGuid HandleId = FNiagaraEditorUtilities::AddEmitterToSystem(
				*System, *Source, Source->GetExposedVersion().VersionGuid);
			auto& Handle = System->GetEmitterHandle(System->GetNumEmitters() - 1);
			check(Handle.GetId() == HandleId);
			Handle.SetName(FName(Layer.Name), *System);
			auto& Data = *Handle.GetEmitterData();
			Data.bLocalSpace = true;
			Data.bDeterminism = true;
			if (Layer.bPlane)
			{
				const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
				const TArray<UNiagaraRendererProperties*> Renderers = Data.GetRenderers();
				for (auto* Renderer : Renderers) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
				auto* Mesh = NewObject<UNiagaraMeshRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				auto* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
				check(Plane);
				Mesh->Meshes.SetNum(1);
				Mesh->Meshes[0].Mesh = Plane;
				Mesh->bOverrideMaterials = true;
				Mesh->OverrideMaterials.AddDefaulted_GetRef().ExplicitMat = Material;
				Instance.Emitter->AddRenderer(Mesh, Instance.Version);
				Mesh->PostEditChange();
			}
			else
			{
				bool bFoundSprite = false;
				for (auto* Renderer : Data.GetRenderers())
				{
					if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
					{
						Sprite->Modify();
						Sprite->Material = Material;
						Sprite->Alignment = Layer.bStreak ? ENiagaraSpriteAlignment::VelocityAligned : ENiagaraSpriteAlignment::Unaligned;
						Sprite->PostEditChange();
						bFoundSprite = true;
					}
				}
				if (!bFoundSprite) return false;
			}
		}
		System->RequestCompile(false);
		System->WaitForCompilationComplete();
		int32 Index = 0;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FLayer& Layer = *(Layers.begin() + Index++);
			Audit += FString::Printf(TEXT("\n%s / %s\n"), Suffix, Layer.Name);
			TuneEmitter(*Handle.GetEmitterData(), Layer, Audit);
		}
		System->PostEditChange();
		System->RequestCompile(true);
		System->WaitForCompilationComplete();
		const bool bSaved = System->IsReadyToRun() && System->IsValid() && UEditorAssetLibrary::SaveLoadedAsset(System, false);
		UE_LOG(LogTemp, Display, TEXT("FOUNDRY_NIAGARA %s EmitterCount=%d Ready=%d"),
			Suffix, System->GetEmitterHandles().Num(), bSaved);
		return bSaved;
	}

	void BuildFoundryParticleSystems()
	{
		FString Audit;
		int32 Built = 0;
		Built += BuildSystem(TEXT("VentPressure"), {
			{TEXT("HotGrateSparks"), TEXT("Fountain"), TEXT("Spark"), 16, .48f, 23, 182, true, false, 22},
			{TEXT("LeakingVapor"), TEXT("Fountain"), TEXT("Steam"), 8, .76f, 55, 92, false, false, 12, 18}}, Audit);
		Built += BuildSystem(TEXT("VentEruption"), {
			{TEXT("UpwardFlame"), TEXT("Fountain"), TEXT("Jet"), 48, .67f, 132, 645, true, false, 19, -20, 18},
			{TEXT("CarriedEmbers"), TEXT("Fountain"), TEXT("Spark"), 29, .63f, 20, 355, true, false, 18, -290},
			{TEXT("ExhaustSmoke"), TEXT("Fountain"), TEXT("Smoke"), 12, 1.05f, 126, 159, false, false, 21, 40}}, Audit);
		Built += BuildSystem(TEXT("VentCooling"), {
			{TEXT("ResidualExhaust"), TEXT("Fountain"), TEXT("Smoke"), 4, 1.f, 66, 87, false, false, 11, 20}}, Audit);
		Built += BuildSystem(TEXT("TroughSteam"), {
			{TEXT("BoilingVapor"), TEXT("Fountain"), TEXT("Steam"), 34, 1.12f, 126, 188, false, false, 66, 25},
			{TEXT("WaterSpray"), TEXT("Fountain"), TEXT("Frost"), 15, .37f, 15, 108, true, false, 61, -320}}, Audit);
		Built += BuildSystem(TEXT("TroughFreeze"), {
			{TEXT("FlashCrystals"), TEXT("OmnidirectionalBurst"), TEXT("Frost"), 36, .69f, 27, 127, false, false, 67, -100},
			{TEXT("ChillCloud"), TEXT("SimpleSpriteBurst"), TEXT("Steam"), 4, .74f, 145, 1}}, Audit);
		Built += BuildSystem(TEXT("TroughConduct"), {
			{TEXT("ArcsAboveWater"), TEXT("Fountain"), TEXT("Charge"), 33, .38f, 82, 70, false, false, 79, -115},
			{TEXT("ShortCorona"), TEXT("Fountain"), TEXT("Frost"), 21, .34f, 15, 119, true, false, 68, -250}}, Audit);
		Built += BuildSystem(TEXT("KegFuse"), {
			{TEXT("WickSparks"), TEXT("Fountain"), TEXT("Spark"), 34, .47f, 30, 177, true, false, 8},
			{TEXT("WickSmoke"), TEXT("Fountain"), TEXT("Smoke"), 4, .67f, 34, 70}}, Audit);
		Built += BuildSystem(TEXT("KegBlast"), {
			{TEXT("FlashAndFlame"), TEXT("OmnidirectionalBurst"), TEXT("Fireball"), 18, .59f, 146, 355, false, false, 32, 110},
			{TEXT("FlyingCinders"), TEXT("OmnidirectionalBurst"), TEXT("Cinder"), 44, .89f, 18, 370, true, false, 37, -390},
			{TEXT("SootRoll"), TEXT("OmnidirectionalBurst"), TEXT("Smoke"), 17, 1.28f, 152, 160, false, false, 32, 70}}, Audit);
		Built += BuildSystem(TEXT("PylonCharge"), {
			{TEXT("CrystalArcs"), TEXT("Fountain"), TEXT("Charge"), 29, .32f, 82, 144, false, false, 20, -30},
			{TEXT("ChargeMotes"), TEXT("Fountain"), TEXT("Frost"), 13, .56f, 12, 111, true}}, Audit);
		Built += BuildSystem(TEXT("PylonDischarge"), {
			{TEXT("MainDischarge"), TEXT("OmnidirectionalBurst"), TEXT("Charge"), 27, .46f, 116, 354, false, false, 26, -60},
			{TEXT("ColdSparks"), TEXT("OmnidirectionalBurst"), TEXT("Frost"), 32, .52f, 20, 340, true, false, 24, -290}}, Audit);
		Built += BuildSystem(TEXT("ArcLink"), {
			{TEXT("ForkedConductorBolt"), TEXT("SimpleSpriteBurst"), TEXT("ArcLink"), 1, .52f, 1, 0, false, true}}, Audit);
		FFileHelper::SaveStringToFile(Audit, *(FPaths::ProjectSavedDir() / TEXT("FoundryNiagaraParameters.tsv")));
		UE_LOG(LogTemp, Display, TEXT("FOUNDRY_NIAGARA_BUILT %d/11"), Built);
	}

	FAutoConsoleCommand BuildFoundryEffectsCommand(TEXT("Bolders.BuildFoundryParticleSystems"),
		TEXT("Author the Foundry hazards' Niagara modules, parameters, and renderers."),
		FConsoleCommandDelegate::CreateStatic(&BuildFoundryParticleSystems));
}

#endif

