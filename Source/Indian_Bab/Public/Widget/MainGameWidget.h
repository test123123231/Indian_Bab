#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainGameWidget.generated.h"

class UButton;
class UDeckLeftWidget;
class UTextBlock;


UCLASS()
class INDIAN_BAB_API UMainGameWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RemainingTime = 20.f;
	
	UPROPERTY(meta=(BindWidget))
	UTextBlock* Time;

	UFUNCTION(BlueprintCallable)
	void OperateTimer();

	
};