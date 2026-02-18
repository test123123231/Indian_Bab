// Copyright Epic Games, Inc. All Rights Reserved.


#include "Indian_BabPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Indian_Bab.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AIndian_BabPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogIndian_Bab, Warning, TEXT("BeginPlay called on PlayerController!"));

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController())
	{
		// 입력 모드 게임 전용으로 설정
		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;
	}
}

void AIndian_BabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
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
