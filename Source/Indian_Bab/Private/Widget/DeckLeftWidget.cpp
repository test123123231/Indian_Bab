// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/DeckLeftWidget.h"

void UDeckLeftWidget::InvisibleWidget() 
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UDeckLeftWidget::VisibleWidget()
{
	SetVisibility(ESlateVisibility::Visible);
	
}