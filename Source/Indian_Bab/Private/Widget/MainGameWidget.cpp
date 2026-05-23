#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Components/TextBlock.h"
#include "PlayerState/MainPlayerState.h"
#include "Components/EditableTextBox.h"
//#include "Components/MultiLineEditableText.h"
#include "Components/Button.h"
#include "Widget/BetProgressWidget.h"
#include "PlayerController\MainGamePlayerController.h"


void UMainGameWidget::NativeDestruct()
{
    // 자기 자신의 BindWidget UObject들 — 재오픈 대비 일괄 해제.
    if (Minus_Button)     Minus_Button->OnClicked.RemoveAll(this);
    if (Plus_Button)      Plus_Button->OnClicked.RemoveAll(this);
    if (Button_Raise)     Button_Raise->OnClicked.RemoveAll(this);
    if (Button_CheckCall) Button_CheckCall->OnClicked.RemoveAll(this);
    if (Button_Fold)      Button_Fold->OnClicked.RemoveAll(this);

    // 외부 객체 구독 해제
    if (MainPS)
    {
        MainPS->OnTriggerCountChanged.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UMainGameWidget::OperateTimer() {
	if (Time) {
		if (RemainingTime-- > 0) {
			Time->SetText(FText::AsNumber(RemainingTime));
		}
	}
}

void UMainGameWidget::NativeConstruct() 
{
	Super::NativeConstruct();

	if (Minus_Button) 
	{
		Minus_Button->OnClicked.AddDynamic(this, &UMainGameWidget::MinusButtonClicked);
	}

	if (Plus_Button) 
	{
		Plus_Button->OnClicked.AddDynamic(this, &UMainGameWidget::PlusButtonClicked);
	}

	// 정리는 NativeDestruct에서 일괄 — Construct는 Add만.
	if (Button_Raise)
	{
		Button_Raise->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonRaise);
	}
	if (Button_CheckCall)
	{
		Button_CheckCall->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonCheckCall);
	}
	if (Button_Fold)
	{
		Button_Fold->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonFold);
	}
	MainGamePC = Cast<AMainGamePlayerController>(GetOwningPlayer());

}

void UMainGameWidget::MinusButtonClicked() {
	if (WBP_BetProgress && BetCount) {
		if (BetNum == 0)
			return;
		if (--BetNum == 0) {
			WBP_BetProgress->Empty();
		}
		else {
			WBP_BetProgress->SetPerCent(0.16f);
		}
		BetCount->SetText(FText::AsNumber(BetNum));	
	}
}

void UMainGameWidget::PlusButtonClicked() {
	if (WBP_BetProgress && BetCount) {
		if (BetNum == 6)
			return;
		if (++BetNum == 6) {
			WBP_BetProgress->Fill();
		}
		else if(BetNum > 0){
			WBP_BetProgress->SetPerCent(-0.16f);
		}
		BetCount->SetText(FText::AsNumber(BetNum));
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


	if (!SubRevolverCount) return;

	SubRevolverCount->SetText(
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
	{
		UE_LOG(LogTemp, Display, TEXT("Click Fold Button"));
		MainGamePC->RequestFold();
	}

}