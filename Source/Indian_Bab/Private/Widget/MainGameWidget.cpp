#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Widget/BetProgressWidget.h"


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