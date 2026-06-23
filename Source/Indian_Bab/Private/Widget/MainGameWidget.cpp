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


void UMainGameWidget::UpdateCenterBetLog(const FString& Message)
{
	if (!Txt_BetLog) return;

	// 1. 텍스트 설정
	Txt_BetLog->SetText(FText::FromString(Message));

	// 2. 기존 타이머가 작동 중이었다면 중복 방지를 위해 초기화
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BetLogTimerHandle);

		// 3. 3.0초 뒤에 ClearBetLog 함수를 호출하도록 타이머 세팅
		GetWorld()->GetTimerManager().SetTimer(
			BetLogTimerHandle,
			this,
			&UMainGameWidget::ClearBetLog,
			3.0f,
			false
		);
	}
}

void UMainGameWidget::ClearBetLog()
{
	if (Txt_BetLog)
	{
		Txt_BetLog->SetText(FText::GetEmpty());
	}
}

void UMainGameWidget::NativeDestruct()
{
	Super::NativeDestruct();
	
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
	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
		WBP_BetProgress->SetPerCent(-0.125f);
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
		WBP_BetProgress->SetPerCent(0.125f);
	}
}

void UMainGameWidget::PlusButtonClicked()
{
	if (BetNum >= 8)
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
		if (BetNum == 8)
		{
			WBP_BetProgress->Fill();
		}
		else
		{
			WBP_BetProgress->SetPerCent(-0.125f);
		}
	}
}

void UMainGameWidget::OnButtonRaise()
{
	if (!MainGamePC) return;
	if (BetNum < 1 || BetNum > 8) return;
	
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
