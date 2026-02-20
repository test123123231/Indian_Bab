#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainGamePlayerController.generated.h"

class UMainGameWidget;

UCLASS()
class INDIAN_BAB_API AMainGamePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    virtual void BeginPlay() override;
    //virtual void SetupInputComponent() override;

    // 위젯에서 호출할 함수(로컬)
    // UFUNCTION(BlueprintCallable)
    // void RequestRaise();

    // UFUNCTION(BlueprintCallable)
    // void RequestCheckCall();

    // UFUNCTION(BlueprintCallable)
    // void RequestFold();

private:
    // 서버로 보내는 RPC
    // UFUNCTION(Server, Reliable)
    // void Server_RequestBetAction(EBetAction Action);

    // UI 생성/관리
    void CreateMainGameWidget();

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UMainGameWidget> MainGameWidgetClass;

    UPROPERTY()
    TObjectPtr<UMainGameWidget> MainGameWidgetInstance;
};
