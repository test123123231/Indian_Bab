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

