#if WITH_EDITOR

#include "EditorAssetLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEmitter.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"

namespace
{
	void AssignAttackParticleMaterials()
	{
		const FString Folder = TEXT("/Game/BoldersDemo/Assets/Combat/Effects/Elemental/");
		const TCHAR* Names[] = {TEXT("Gold"), TEXT("Steel"), TEXT("Ricochet"), TEXT("Ember"),
			TEXT("Flame"), TEXT("Frost"), TEXT("Foam"), TEXT("Thunder")};
		for (const TCHAR* Name : Names)
		{
			auto* System = LoadObject<UNiagaraSystem>(nullptr, *(Folder + TEXT("NS_Impact_") + Name));
			auto* Material = LoadObject<UMaterialInterface>(nullptr, *(Folder + TEXT("M_Particle_") + Name));
			if (!System || !Material) continue;
			int32 RendererCount = 0;
			for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
			{
				const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
				if (!Data) continue;
				for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
				{
					if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
					{
						Sprite->Modify();
						Sprite->Material = Material;
						Sprite->PostEditChange();
						++RendererCount;
					}
					else if (auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
					{
						Ribbon->Modify();
						Ribbon->Material = Material;
						Ribbon->PostEditChange();
						++RendererCount;
					}
				}
			}
			if (RendererCount == 0)
			{
				UE_LOG(LogTemp, Error, TEXT("AttackParticles: %s has no compatible renderer"), Name);
				continue;
			}
			System->PostEditChange();
			System->RequestCompile(false);
			System->WaitForCompilationComplete(false, false);
			UEditorAssetLibrary::SaveLoadedAsset(System, false);
			UE_LOG(LogTemp, Log, TEXT("AttackParticles: %s renderers=%d"), Name, RendererCount);
		}
	}

	FAutoConsoleCommand AssignAttackParticleMaterialsCommand(TEXT("Bolders.AssignAttackParticleMaterials"),
		TEXT("Apply project combat particle materials to the generated Niagara systems."),
		FConsoleCommandDelegate::CreateStatic(&AssignAttackParticleMaterials));
}

#endif
