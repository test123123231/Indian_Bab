#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"


class UInputMappingContext;
class UMainMenuWidget;


UCLASS()
class INDIAN_BAB_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenMainMenu();

	/** * 생성할 메인 메뉴 위젯의 블루프린트 클래스
	 * TSubclassOf는 특정 클래스(여기서는 UUserWidget)의 자식 클래스만 할당할 수 있도록 제한
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

	// 생성된 메인 메뉴 위젯의 인스턴스를 저장할 변수
	UPROPERTY(VisibleInstanceOnly, Category = "UI")
	TObjectPtr<UMainMenuWidget> MainMenuWidgetInstance;

	//--- 연결성(인터넷 끊김) ---

	// 끊김 시 모달로 띄울 위젯 클래스 (BP에서 WBP_OfflineNotice 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Connectivity")
	TSubclassOf<UUserWidget> OfflineWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> OfflineWidgetInstance;

private:
	void HandleConnectivityLost();
	void HandleConnectivityRestored();

	FDelegateHandle LostHandle;
	FDelegateHandle RestoredHandle;
};
