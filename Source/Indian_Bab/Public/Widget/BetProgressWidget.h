// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BetProgressWidget.generated.h"

UCLASS()
class INDIAN_BAB_API UBetProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	UFUNCTION()
	void SetPerCent(float num);

	void Fill();
	void Empty();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Value;

};
