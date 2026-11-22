#if WITH_EDITOR
#include "AI/Positioning/EQS/AttackPositionQuery.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Donut.h"
#include "DataProviders/AIDataProvider_QueryParams.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "HAL/IConsoleManager.h"

namespace
{
	void BindRadius(UEnvQueryGenerator_Donut* Generator, FAIDataProviderFloatValue& Value, FName Name)
	{
		auto* Binding = NewObject<UAIDataProvider_QueryParams>(Generator);
		Binding->ParamName = Name;
		Value.DataBinding = Binding;
		Value.DataField = TEXT("FloatValue");
	}

	UEnvQueryGenerator_Donut* CreateGenerator(UEnvQuery* Query)
	{
		auto* Generator = NewObject<UEnvQueryGenerator_Donut>(Query);
		Generator->Center = UAttackTargetContext::StaticClass();
		Generator->PointOnRingSpacingMethod = EEnvQueryPointSpacingMethod::ByNumberOfPoints;
		Generator->NumberOfRings.DefaultValue = 2;
		Generator->PointsPerRing.DefaultValue = 12;
		Generator->InnerRadius.DefaultValue = 138.f;
		Generator->OuterRadius.DefaultValue = 153.f;
		Generator->ProjectionData.TraceMode = EEnvQueryTrace::Navigation;
		Generator->ProjectionData.ProjectUp = 150.f;
		Generator->ProjectionData.ProjectDown = 200.f;
		BindRadius(Generator, Generator->InnerRadius, TEXT("InnerRadius"));
		BindRadius(Generator, Generator->OuterRadius, TEXT("OuterRadius"));
		return Generator;
	}

	UEnvQueryTest_AttackPath* CreatePathTest(UEnvQuery* Query)
	{
		auto* Test = NewObject<UEnvQueryTest_AttackPath>(Query);
		Test->TestOrder = 1;
		return Test;
	}

	UEnvQueryOption* CreateOption(UEnvQuery* Query)
	{
		auto* Option = NewObject<UEnvQueryOption>(Query);
		Option->Generator = CreateGenerator(Query);
		auto* Geometry = NewObject<UEnvQueryTest_AttackPosition>(Query);
		Geometry->TestOrder = 0;
		Option->Tests.Add(Geometry);
		Option->Tests.Add(CreatePathTest(Query));
		return Option;
	}
}

static void BuildAttackQuery()
{
	const TCHAR* Path = TEXT("/Game/BoldersDemo/Assets/Combat/EQS_FindAttackPosition");
	auto* Query = LoadObject<UEnvQuery>(nullptr, Path);
	if (!Query)
	{
		Query = NewObject<UEnvQuery>(CreatePackage(Path), TEXT("EQS_FindAttackPosition"), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Query);
	}
	Query->Modify();
	Query->EdGraph = nullptr; // Reconstructed from the new options when opened in the editor.
	Query->GetOptionsMutable().Empty();
	Query->GetOptionsMutable().Add(CreateOption(Query));
	Query->MarkPackageDirty();
	UEditorAssetLibrary::SaveLoadedAsset(Query, false);
	UE_LOG(LogTemp, Log, TEXT("AttackEQS: query asset saved"));
}
static FAutoConsoleCommand BuildAttackQueryCommand(TEXT("Bolders.BuildAttackQuery"),
	TEXT("Rebuild attack-position EQS asset with project defaults"), FConsoleCommandDelegate::CreateStatic(&BuildAttackQuery));
#endif
