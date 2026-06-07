#include "Widget/GameResultWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"

void UGameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Ready)
	{
		Button_Ready->OnClicked.AddDynamic(this, &UGameResultWidget::OnBackToMainMenuClicked);
	}
}

void UGameResultWidget::NativeDestruct()
{
	if (Button_Ready)
	{
		Button_Ready->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UGameResultWidget::SetResult(const FString& WinnerName, bool /*bLocalPlayerWon*/)
{
	if (Text_WinnerText)
	{
		Text_WinnerText->SetText(FText::FromString(WinnerName));
	}
}

void UGameResultWidget::ConfirmBackToMainMenu()
{
	OnBackToMainMenuClicked();
}

void UGameResultWidget::OnBackToMainMenuClicked()
{
	if (bMainMenuTravelRequested || MainMenuMapPath.IsEmpty())
	{
		return;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		bMainMenuTravelRequested = true;
		if (UGameInstance* GameInstance = PC->GetGameInstance())
		{
			if (USessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USessionSubsystem>())
			{
				SessionSubsystem->ReturnToMainMenuAfterSessionCleanup(MainMenuMapPath);
				return;
			}
		}

		PC->ClientTravel(MainMenuMapPath, ETravelType::TRAVEL_Absolute);
	}
}
