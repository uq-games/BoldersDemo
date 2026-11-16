#include "Player/BoldersPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

void ABoldersPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}
	if (!CameraMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("BoldersPlayerController: CameraMappingContext is not assigned."));
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(CameraMappingContext, 0);
			MappingLocalPlayer = LocalPlayer;
		}
	}
}

void ABoldersPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* LocalPlayer = MappingLocalPlayer.Get())
	{
		if (CameraMappingContext)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->RemoveMappingContext(CameraMappingContext);
			}
		}
	}
	MappingLocalPlayer.Reset();

	Super::EndPlay(EndPlayReason);
}
