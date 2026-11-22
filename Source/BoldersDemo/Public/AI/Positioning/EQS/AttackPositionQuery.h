#pragma once
#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "AttackPositionQuery.generated.h"

UCLASS()
class BOLDERSDEMO_API UAttackTargetContext : public UEnvQueryContext
{
 GENERATED_BODY()
public:
 virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

// Checks the standing space and attack geometry before the more expensive path test.
UCLASS()
class BOLDERSDEMO_API UEnvQueryTest_AttackPosition : public UEnvQueryTest
{
 GENERATED_BODY()
public:
 UEnvQueryTest_AttackPosition();
 virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
};

// One path query per candidate: reachability, target clearance and path-length score.
UCLASS()
class BOLDERSDEMO_API UEnvQueryTest_AttackPath : public UEnvQueryTest
{
	GENERATED_BODY()
public:
	UEnvQueryTest_AttackPath();
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
};
