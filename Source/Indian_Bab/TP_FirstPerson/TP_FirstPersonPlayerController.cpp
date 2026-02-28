#include "TP_FirstPersonPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "TP_FirstPersonCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Indian_Bab.h"
#include "TP_FirstPerson.h"
#include "Widgets/Input/SVirtualJoystick.h"

ATP_FirstPersonPlayerController::ATP_FirstPersonPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ATP_FirstPersonCameraManager::StaticClass();
}

void ATP_FirstPersonPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController())
	{
		FInputModeGameOnly InputModeData;
		SetInputMode(InputModeData);

		bShowMouseCursor = false;
	}
}

void ATP_FirstPersonPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}
