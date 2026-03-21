


#include "Widget/BetProgressWidget.h"

void UBetProgressWidget::SetPerCent(float num) {
	Value -= num;
}

void UBetProgressWidget::Fill() {
	Value = 1.0f;
}

void UBetProgressWidget::Empty() {
	Value = 0.0f;
}