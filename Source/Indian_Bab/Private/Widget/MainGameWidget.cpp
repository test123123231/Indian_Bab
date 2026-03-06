#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Components/TextBlock.h"


void UMainGameWidget::OperateTimer() {
	if (Time) {
		if (RemainingTime-- > 0) {
			Time->SetText(FText::AsNumber(RemainingTime));
		}
	}
}
