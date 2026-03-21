#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widget/BetProgressWidget.h"
#include "MainGameWidget.generated.h"


class UButton;
class UDeckLeftWidget;
class UTextBlock;
class UBetProgressWidget;

UCLASS()
class INDIAN_BAB_API UMainGameWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RemainingTime = 20.f;

	UPROPERTY(EditAnywhere)
	float BetNum = 0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Time;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BetCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Plus_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Minus_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBetProgressWidget> WBP_BetProgress;

	UFUNCTION(BlueprintCallable)
	void OperateTimer();

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void MinusButtonClicked();

	UFUNCTION()
	void PlusButtonClicked();


};