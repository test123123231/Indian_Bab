#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameResultWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class INDIAN_BAB_API UGameResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetResult(const FString& WinnerName, bool bLocalPlayerWon);
	void ConfirmBackToMainMenu();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_WinnerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Ready;

	UPROPERTY(EditDefaultsOnly, Category = "Result")
	FString MainMenuMapPath = TEXT("/Game/Maps/MainMenu/MainMenu");

	bool bMainMenuTravelRequested = false;

	UFUNCTION()
	void OnBackToMainMenuClicked();
};
