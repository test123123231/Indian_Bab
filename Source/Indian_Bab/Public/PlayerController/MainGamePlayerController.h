#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"   
#include "MainGamePlayerController.generated.h"

class UMainGameWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class INDIAN_BAB_API AMainGamePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // 서버 호출
    UFUNCTION(BlueprintCallable)
    void RequestRaise();

    UFUNCTION(BlueprintCallable)
    void RequestCheckCall();

    UFUNCTION(BlueprintCallable)
    void RequestFold();

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

    // Input 관리
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_Raise;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_CheckCall;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_Fold;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_RMB;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> IA_Look;


    // 카메라 모드/UI 모드 관리
    bool bRMBHeld = false;

    void OnRMBPressed(const FInputActionValue& Value);
    void OnRMBReleased(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);

    void EnterUIMode();     // 커서 보이기
    void EnterCameraMode(); // 커서 숨기고 회전
};
