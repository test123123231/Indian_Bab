// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckLeftWidget.generated.h"

class AMainGamePlayerController;
class AMainPlayerState;

UCLASS()
class INDIAN_BAB_API UDeckLeftWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	TObjectPtr<AMainGamePlayerController> MainGamePC;

	UPROPERTY()
	TObjectPtr<AMainPlayerState> MainPS;

	void InvisibleWidget();

	void VisibleWidget();
};
