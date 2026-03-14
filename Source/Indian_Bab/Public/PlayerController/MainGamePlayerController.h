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
	AMainGamePlayerController();

    virtual void BeginPlay() override;

    void ApplyLobbyMappingContext();

    void ApplyMainGameMappingContext();

	virtual void SetupInputComponent() override;

    // 서버 호출
    UFUNCTION(BlueprintCallable)
    void RequestRaise();

    UFUNCTION(BlueprintCallable)
    void RequestCheckCall();

    UFUNCTION(BlueprintCallable)
    void RequestFold();

    UFUNCTION(Client, Reliable)
    void ClientOnSeated();

private:
    // 서버로 보내는 RPC
    // UFUNCTION(Server, Reliable)
    // void Server_RequestBetAction(EBetAction Action);

	// 입력 모드 전환 함수
    void EnterUIMode();     // 커서 보이기
    void EnterCameraMode(); // 커서 숨기고 회전

    // UI 생성/관리
    void CreateMainGameWidget();

	// 입력 바인딩 함수
    void OnMainGameLook(const FInputActionValue& Value);

    void OnMainGameRMBPressed(const FInputActionValue& Value);

    void OnMainGameRMBReleased(const FInputActionValue& Value);

    void OnMainGameCheckCall(const FInputActionValue& Value);

    void OnMainGameFold(const FInputActionValue& Value);

    void OnMainGameRaise(const FInputActionValue& Value);

    void OnLobbyMove(const FInputActionValue& Value);

    void OnLobbyLook(const FInputActionValue& Value);

    // 감도
    float LookSensitivity = 1.0f;

    // 카메라 모드/UI 모드 관리
    bool bRMBHeld = false;

   	// UI 위젯 클래스와 인스턴스 참조 변수
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UMainGameWidget> MainGameWidgetClass;

    UPROPERTY()
    TObjectPtr<UMainGameWidget> MainGameWidgetInstance;

    // Input 관리
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> MainGameMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> LobbyMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameRMB;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameLook;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameCheckCall;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameFold;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameRaise;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_LobbyMove;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_LobbyLook;

};
