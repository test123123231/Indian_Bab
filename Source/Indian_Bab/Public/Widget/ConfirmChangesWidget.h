#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfirmChangesWidget.generated.h"


class UButton;
class UOptionMenuWidget;


/**
 * UOptionMenuWidget에서 변경 사항이 있을 때 띄우는 '예/아니오' 팝업 위젯
 */
UCLASS()
class INDIAN_BAB_API UConfirmChangesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetParentMenu(UUserWidget* InParentMenu);

protected:
	// 위젯 생성 시 (EventConstruct) 호출
	virtual void NativeConstruct() override;

private:
	//--- BP 위젯 변수 바인딩 ---

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Yes;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_No;

	//--- C++ 이벤트 핸들러 ---

	UFUNCTION()
	void OnYesClicked();

	UFUNCTION()
	void OnNoClicked();

	//--- 캐시된 참조 ---

	// NativeConstruct에서 GetOwningUser()를 통해 캐시되는 부모 위젯
	UPROPERTY()
	TObjectPtr<UOptionMenuWidget> OptionMenuOwner;
};
