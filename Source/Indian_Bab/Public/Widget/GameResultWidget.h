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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ReadyState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Ready;

	UPROPERTY(EditDefaultsOnly, Category = "Result")
	FName MainMenuMapName = TEXT("/Game/Maps/MainMenu/MainMenu");

	UFUNCTION()
	void OnBackToMainMenuClicked();
};
