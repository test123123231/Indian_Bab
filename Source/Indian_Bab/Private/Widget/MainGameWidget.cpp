#include "Widget/MainGameWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableText.h" 

void UMainGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// C++ 함수와 BP 버튼의 OnClicked 이벤트를 바인딩
	if (Button_Raise)
	{
        Button_Raise->OnClicked.RemoveAll(this);
		Button_Raise->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonRaise);
	}
	if (Button_CheckCall)
	{
        Button_CheckCall->OnClicked.RemoveAll(this);
		Button_CheckCall->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonCheckFall);
	}
	if (Button_Fold)
	{
        Button_Fold->OnClicked.RemoveAll(this);
		Button_Fold->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonFold);
	}
    
    RefreshUI();
}

void UMainGameWidget::NativeDestruct()
{
	Super::NativeDestruct();

    // 위젯 제거될 때 이벤트 정리
    if (Button_Raise)     Button_Raise->OnClicked.RemoveAll(this);
    if (Button_CheckCall) Button_CheckCall->OnClicked.RemoveAll(this);
    if (Button_Fold)      Button_Fold->OnClicked.RemoveAll(this);
}

void UMainGameWidget::RefreshUI()
{

}

void UMainGameWidget::OnButtonRaise()
{

}

void UMainGameWidget::OnButtonCheckFall()
{

}

void UMainGameWidget::OnButtonFold()
{
    
}
