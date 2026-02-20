// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainGameWidget.generated.h"

class UButton;
class UMultiLineEditableText;
class UEditableTextBox;
class UTextBlock;


UCLASS()
class INDIAN_BAB_API UMainGameWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UPROPERTY(meta = (BindWidget))
	// TObjectPtr<UEditableTextBox> Text_SubRevolverCount;

	// UPROPERTY(meta = (BindWidget))
	// TObjectPtr<UEditableTextBox> Text_PlusTokenCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Time;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_CheckCall;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Raise;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CheckCall;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Fold;

	// 레이즈 버튼 클릭 시
	UFUNCTION()
	void OnButtonRaise();

	// 체크/콜 버튼 클릭 시
	UFUNCTION()
	void OnButtonCheckCall();

	// 폴드 버튼 클릭 시
	UFUNCTION()
	void OnButtonFold();

	void RefreshUI();
};


