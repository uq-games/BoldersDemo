#pragma once
#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

UCLASS()
class BOLDERSDEMO_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()
public:
	AEnemyCharacter();
#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
#endif
};
