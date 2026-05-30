#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReadyWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class INDIAN_BAB_API UReadyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:
	void ConfirmReady();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Ready;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ReadyState;

	bool bReadySubmitted = false;

	UFUNCTION()
	void OnReadyButtonClicked();
};
