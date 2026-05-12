#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Components/TextBlock.h"
#include "PlayerState/MainPlayerState.h"
#include "Components/EditableTextBox.h"
//#include "Components/MultiLineEditableText.h" 
#include "Components/Button.h"
#include "Widget/BetProgressWidget.h"
#include "Game/MainGameState.h"
#include "PlayerController\MainGamePlayerController.h"


void UMainGameWidget::NativeDestruct()
{
    // 위젯 제거될 때 이벤트 정리
    if (Button_Raise)     Button_Raise->OnClicked.RemoveAll(this);
    if (Button_CheckCall) Button_CheckCall->OnClicked.RemoveAll(this);
    if (Button_Fold)      Button_Fold->OnClicked.RemoveAll(this);

    if (MainPS)
    {
        MainPS->OnTriggerCountChanged.RemoveAll(this);
    }

}

void UMainGameWidget::OperateTimer() {
	if (!Time) return;

	AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (!GS) return;

	if (!GS->bTimerActive)
	{
		Time->SetText(FText::FromString(TEXT("-")));
		return;
	}

	Time->SetText(FText::AsNumber(GS->GetRemainingTimeCeil()));
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
	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
		WBP_BetProgress->SetPerCent(-0.16f);
	}
	MainGamePC = Cast<AMainGamePlayerController>(GetOwningPlayer());

}

void UMainGameWidget::MinusButtonClicked()
{
	if (BetNum <= 1)
	{
		return;
	}

	BetNum--;

	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
	}

	if (WBP_BetProgress)
	{
		WBP_BetProgress->SetPerCent(0.16f);
	}
}

void UMainGameWidget::PlusButtonClicked()
{
	if (BetNum >= 6)
	{
		return;
	}

	BetNum++;

	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
	}

	if (WBP_BetProgress)
	{
		if (BetNum == 6)
		{
			WBP_BetProgress->Fill();
		}
		else
		{
			WBP_BetProgress->SetPerCent(-0.16f);
		}
	}
}

void UMainGameWidget::OnButtonRaise()
{
	if (!MainGamePC) return;
	if (BetNum < 1 || BetNum > 6) return;
	
	UE_LOG(LogTemp, Display, TEXT("Click Raise Button"));
	MainGamePC->RequestRaise(BetNum);
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
}

int32 UMainGameWidget::GetBetNum() const
{
	return BetNum;
}