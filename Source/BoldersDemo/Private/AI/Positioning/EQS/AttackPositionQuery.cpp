#include "AI/Positioning/EQS/AttackPositionQuery.h"
#include "AI/Positioning/AttackPositioningComponent.h"
#include "Characters/BaseCharacter.h"
#include "AI/Positioning/AttackPositionRules.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	const UAttackPositioningComponent* GetPositioning(const FEnvQueryInstance& Query)
	{
		const auto* Owner = Cast<AActor>(Query.Owner.Get());
		return Owner ? Owner->FindComponentByClass<UAttackPositioningComponent>() : nullptr;
	}
}

void UAttackTargetContext::ProvideContext(FEnvQueryInstance& Query, FEnvQueryContextData& Data) const
{
	const auto* Positioning = GetPositioning(Query);
	if (Positioning && IsValid(Positioning->GetTarget()))
	{
		UEnvQueryItemType_Actor::SetContextHelper(Data, Positioning->GetTarget());
	}
}

UEnvQueryTest_AttackPosition::UEnvQueryTest_AttackPosition()
{
	Cost = EEnvTestCost::Medium;
	ValidItemType = UEnvQueryItemType_Point::StaticClass();
	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Match;
	BoolValue.DefaultValue = true;
	SetWorkOnFloatValues(false);
}

void UEnvQueryTest_AttackPosition::RunTest(FEnvQueryInstance& Query) const
{
	const auto* Positioning = GetPositioning(Query);
	for (FEnvQueryInstance::ItemIterator It(this, Query); It; ++It)
	{
		const bool bValid = Positioning
			&& Positioning->IsCandidateValid(GetItemLocation(Query, It.GetIndex()));
		// This test is an invariant: only valid standing/attack positions may survive.
		It.SetScore(EEnvTestPurpose::Filter, EEnvTestFilterType::Match, bValid, true);
	}
}

UEnvQueryTest_AttackPath::UEnvQueryTest_AttackPath()
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_Point::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::FilterAndScore;
	FilterType = EEnvTestFilterType::Minimum;
	FloatValueMin.DefaultValue = 0.f;
	ScoringEquation = EEnvTestScoreEquation::InverseLinear;
}

void UEnvQueryTest_AttackPath::RunTest(FEnvQueryInstance& Query) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BoldersAttackPathQuery);
	const auto* Character = Cast<ABaseCharacter>(Query.Owner.Get());
	const auto* Positioning = GetPositioning(Query);
	const auto* Target = Positioning ? Positioning->GetTarget() : nullptr;
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Query.World);
	const auto* NavData = Character && Navigation
		? Navigation->GetNavDataForProps(Character->GetNavAgentPropertiesRef(), Character->GetNavAgentLocation()) : nullptr;
	if (!Target || !NavData)
	{
		for (FEnvQueryInstance::ItemIterator It(this, Query); It; ++It)
			It.ForceItemState(EEnvItemStatus::Failed);
		return;
	}

	const auto Filter = UNavigationQueryFilter::GetQueryFilter(*NavData, Character, nullptr);
	const FVector Start = Character->GetNavAgentLocation();
	const AttackPositionRules::FApproachWarnings ApproachWarnings(*Character);
	TArray<FVector> Points;
	for (FEnvQueryInstance::ItemIterator It(this, Query); It; ++It)
	{
		FPathFindingQuery PathQuery(Character, *NavData, Start, GetItemLocation(Query, It.GetIndex()), Filter);
		PathQuery.SetAllowPartialPaths(false);
		const FPathFindingResult Result = Navigation->FindPathSync(Character->GetNavAgentPropertiesRef(), PathQuery);
		if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
		{
			It.ForceItemState(EEnvItemStatus::Failed);
			continue;
		}
		Points.Reset(Result.Path->GetPathPoints().Num());
		for (const FNavPathPoint& Point : Result.Path->GetPathPoints()) Points.Add(Point.Location);
		if (!AttackPositionRules::IsPathClearOfTarget(*Character, *Target, Points)
			|| !ApproachWarnings.IsPathSafe(Points))
		{
			It.ForceItemState(EEnvItemStatus::Failed);
			continue;
		}
		It.SetScore(EEnvTestPurpose::Score, EEnvTestFilterType::Minimum, Result.Path->GetLength(), 0.f, MAX_flt);
	}
}
