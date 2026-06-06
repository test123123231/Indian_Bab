#include "PlayerController/MainMenuPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "Widget/MainMenuWidget.h"
#include "Character/LobbyVRCharacter.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}

	if (!IsLocalPlayerController())
		return;

	ApplyMainMenuMappingContext();
	OpenMainMenu();

	// 연결성 구독 + 폴링 시작 — 메인메뉴 PC 살아있는 동안만 활성
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			LostHandle = Connectivity->OnConnectivityLost.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityLost);
			RestoredHandle = Connectivity->OnConnectivityRestored.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityRestored);

			Connectivity->StartPolling();
		}
	}
}

void AMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 델리게이트 해제 + 폴링 정지 — PC 파괴 시 dangling 핸들 방지
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			Connectivity->OnConnectivityLost.Remove(LostHandle);
			Connectivity->OnConnectivityRestored.Remove(RestoredHandle);
			Connectivity->StopPolling();
		}
	}

	if (IsLocalPlayerController())
	{
		RemoveMainMenuMappingContext();
	}

	Super::EndPlay(EndPlayReason);
}

void AMainMenuPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

	if (!IsLocalPlayerController()) 
        return;

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_RightTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick bound"));
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Started, this, &AMainMenuPlayerController::OnRightTriggerClickStarted);
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Completed, this, &AMainMenuPlayerController::OnRightTriggerClickReleased);
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Canceled, this, &AMainMenuPlayerController::OnRightTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick is null"));
        }

        if (IA_LeftTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick bound"));
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Started, this, &AMainMenuPlayerController::OnLeftTriggerClickStarted);
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Completed, this, &AMainMenuPlayerController::OnLeftTriggerClickReleased);
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Canceled, this, &AMainMenuPlayerController::OnLeftTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick is null"));
        }
    }
}

void AMainMenuPlayerController::ApplyMainMenuMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyMainMenuMappingContext failed. LocalPlayer is null"));
		return;
	}

	auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyMainMenuMappingContext failed. EnhancedInput subsystem is null"));
		return;
	}

	if (MainMenuMappingContext)
	{
		Subsys->AddMappingContext(MainMenuMappingContext, 0);
		UE_LOG(LogTemp, Warning, TEXT("[Input] MainMenuMappingContext applied: %s"), *GetNameSafe(MainMenuMappingContext));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] MainMenuMappingContext is null"));
	}
}

void AMainMenuPlayerController::RemoveMainMenuMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !MainMenuMappingContext)
	{
		return;
	}

	if (auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		Subsys->RemoveMappingContext(MainMenuMappingContext);
	}
}

void AMainMenuPlayerController::OnLeftTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressLeftWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnLeftTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseLeftWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnRightTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressRightWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnRightTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseRightWidgetInteraction();
	}
}

void AMainMenuPlayerController::OpenMainMenu()
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->SetActiveVRUI(EVRActiveUI::MainMenu);
	}

	if (!MainMenuWidgetClass)
	{
		return;
	}

	MainMenuWidgetInstance = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (MainMenuWidgetInstance)
	{
		ApplyMenuInputMode(MainMenuWidgetInstance);
	}
}

void AMainMenuPlayerController::ApplyMenuInputMode(UUserWidget* FocusWidget)
{
	const bool bUseMouseInput = MenuInputMode == EMainMenuInputMode::Mouse;

	if (bUseMouseInput)
	{
		if (MainMenuWidgetInstance && !MainMenuWidgetInstance->IsInViewport())
		{
			MainMenuWidgetInstance->AddToViewport();
		}

		FInputModeUIOnly InputModeData;
		if (FocusWidget)
		{
			InputModeData.SetWidgetToFocus(FocusWidget->TakeWidget());
		}
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);

		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		return;
	}

	if (MainMenuWidgetInstance && MainMenuWidgetInstance->IsInViewport())
	{
		MainMenuWidgetInstance->RemoveFromParent();
	}

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AMainMenuPlayerController::SetMenuInputMode(EMainMenuInputMode NewInputMode)
{
	MenuInputMode = NewInputMode;
	ApplyMenuInputMode(MainMenuWidgetInstance);
}

void AMainMenuPlayerController::HandleConnectivityLost()
{
	if (!OfflineWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: OfflineWidgetClass is not set."));
		return;
	}

	if (!OfflineWidgetInstance)
	{
		OfflineWidgetInstance = CreateWidget<UUserWidget>(this, OfflineWidgetClass);
		if (OfflineWidgetInstance)
		{
			OfflineWidgetInstance->SetIsFocusable(true);
		}
	}

	if (OfflineWidgetInstance && !OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->AddToViewport(100);

		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(OfflineWidgetInstance->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
	}
}

void AMainMenuPlayerController::RefocusMainMenu()
{
	if (MainMenuWidgetInstance)
	{
		ApplyMenuInputMode(MainMenuWidgetInstance);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: RefocusMainMenu skipped because MainMenuWidget is missing."));
	}
}

void AMainMenuPlayerController::HandleConnectivityRestored()
{
	if (OfflineWidgetInstance && OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->RemoveFromParent();
		RefocusMainMenu();
	}
}
