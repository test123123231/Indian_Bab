#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoomJoinWidget.generated.h"

class UButton;
class UTextBlock;
class UEditableTextBox;

UCLASS()
class INDIAN_BAB_API URoomJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetParentMenu(UUserWidget* InParentMenu);

protected:
	// 위젯 생성 시 (EventConstruct) 호출
	virtual void NativeConstruct() override;

	/**
	 * 이 위젯이 포커스를 가지고 있을 때 키 입력을 받음 (ESC 키 처리용)
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * [추가] 이 위젯에서 마우스 버튼이 눌렸을 때 호출 (포커스 유지용)
	 * 이 위젯의 '배경'을 클릭해도 포커스를 잃지 않도록 함
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Yes;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_No;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> InviteCodeTextBox;
	//--- 상태 변수 ---
	UPROPERTY() TObjectPtr<APlayerController> PlayerControllerRef;
	UPROPERTY() TObjectPtr<UUserWidget> ParentMenu;

	UFUNCTION() void OnYesClicked();
	UFUNCTION() void OnNoClicked();
};
