#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widget/BetProgressWidget.h"
#include "MainGameWidget.generated.h"


class UButton;
class UDeckLeftWidget;
class UTextBlock;
class UBetProgressWidget;
class AMainGamePlayerController;

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
	TObjectPtr<UButton> Button_Raise;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CheckCall;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Fold;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBetProgressWidget> WBP_BetProgress;

	UFUNCTION(BlueprintCallable)
	void OperateTimer();

	// 플레이어 컨트롤러 참조 변수
	TObjectPtr<AMainGamePlayerController> MainGamePC;

	// 레이즈 버튼 클릭 시
	UFUNCTION()
	void OnButtonRaise();

	// 체크/콜 버튼 클릭 시
	UFUNCTION()
	void OnButtonCheckCall();

	// 폴드 버튼 클릭 시
	UFUNCTION()
	void OnButtonFold();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void MinusButtonClicked();

	UFUNCTION()
	void PlusButtonClicked();


};

