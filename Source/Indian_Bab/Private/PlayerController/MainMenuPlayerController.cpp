#include "PlayerController/MainMenuPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Widget/MainMenuWidget.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}

	if (!IsLocalPlayerController())
		return;

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

	Super::EndPlay(EndPlayReason);
}

void AMainMenuPlayerController::OpenMainMenu()
{
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
