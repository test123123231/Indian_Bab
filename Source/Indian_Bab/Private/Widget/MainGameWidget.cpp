#include "Widget/MainGameWidget.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "PlayerState/MainPlayerState.h"
//#include "Components/EditableTextBox.h"
//#include "Components/MultiLineEditableText.h" 

void UMainGameWidget::NativeDestruct()
{
	Super::NativeDestruct();

    // 위젯 제거될 때 이벤트 정리
    if (Button_Raise)     Button_Raise->OnClicked.RemoveAll(this);
    if (Button_CheckCall) Button_CheckCall->OnClicked.RemoveAll(this);
    if (Button_Fold)      Button_Fold->OnClicked.RemoveAll(this);

    if (MainPS)
    {
        MainPS->OnTriggerCountChanged.RemoveAll(this);
    }
}

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
		Button_CheckCall->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonCheckCall);
	}
	if (Button_Fold)
	{
        Button_Fold->OnClicked.RemoveAll(this);
		Button_Fold->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonFold);
	}
}

void UMainGameWidget::OnButtonRaise()
{
	if (MainGamePC)
    {
        UE_LOG(LogTemp, Display, TEXT("Click Raise Button"));
        MainGamePC->RequestRaise();
    }
}

void UMainGameWidget::OnButtonCheckCall()
{
    if (MainGamePC)
    {
        UE_LOG(LogTemp, Display, TEXT("Click CheckCall Button"));
        MainGamePC->RequestCheckCall();
    }
}

void UMainGameWidget::OnButtonFold()
{
	if (MainGamePC)
    {
        UE_LOG(LogTemp, Display, TEXT("Click Fold Button"));
        MainGamePC->RequestFold();
    }
}


void UMainGameWidget::UpdateSubRevolverCount(int32 Count)
{
    //UE_LOG(LogTemp, Warning, TEXT("[Widget] UpdateSubRevolverCount called : %d"), Count);


	if (!Text_SubRevolverCount) return;

	Text_SubRevolverCount->SetText(
		FText::FromString(FString::Printf(TEXT("%d"), 8 - Count))
	);
}

void UMainGameWidget::InitWidget()
{    
    MainGamePC = Cast<AMainGamePlayerController>(GetOwningPlayer());
    if (!MainGamePC) return;

    MainPS = MainGamePC->GetPlayerState<AMainPlayerState>();
    if (!MainPS) return;

    MainPS->OnTriggerCountChanged.RemoveAll(this);
    MainPS->OnTriggerCountChanged.AddUObject(this, &UMainGameWidget::UpdateSubRevolverCount);

    UpdateSubRevolverCount(MainPS->TotalTriggerCount);
    UE_LOG(LogTemp, Warning, TEXT("[Widget] InitWidget success"));
}