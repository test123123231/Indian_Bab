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

	//--- 매치메이커/데디 거부 모달 ---

	// 매치메이커 실패·데디 PreLogin 거부 사유를 띄울 위젯 (BP에서 WBP_SessionErrorNotice 지정)
	// 위젯은 BindWidget으로 TextBlock(MessageText) 노출하고, 호스트에서 SetText로 사유 주입.
	UPROPERTY(EditDefaultsOnly, Category = "Session")
	TSubclassOf<UUserWidget> SessionErrorWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> SessionErrorWidgetInstance;

public:
	// 모달 위젯(WBP_SessionErrorNotice 등)이 자기 자신을 RemoveFromParent로 닫은 뒤 호출.
	// MainMenuWidget이 살아있으면 거기로 입력 포커스/모드를 되돌린다.
	// 모달의 RemoveFromParent는 호출 측 BP가 책임 — 이 함수는 포커스 복원만 담당.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefocusMainMenu();

private:
	void HandleConnectivityLost();
	void HandleConnectivityRestored();

	// 매치메이커 사유(HTTP/파싱/필드 누락) 및 데디 거부(GameInstance::OnTravelRejected) 공용 핸들러
	UFUNCTION()
	void HandleSessionError(const FString& Reason);

	FDelegateHandle LostHandle;
	FDelegateHandle RestoredHandle;
};
