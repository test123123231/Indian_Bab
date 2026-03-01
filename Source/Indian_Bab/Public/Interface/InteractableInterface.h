// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class INDIAN_BAB_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// 상호작용 실행 함수 (상호작용을 시도한 액터를 매개변수로 받음)
	// BlueprintNativeEvent를 사용하여 C++과 블루프린트 양쪽에서 구현 가능하게 만듭니다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	// 화면에 띄울 상호작용 텍스트 (예: "F를 눌러 앉기", "F를 눌러 리볼버 집기")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt();
};
