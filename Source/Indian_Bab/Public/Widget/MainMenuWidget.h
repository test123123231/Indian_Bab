#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"


class UButton;
class UOptionMenuWidget;
class URoomCreateWidget;
class URoomJoinWidget;


/**
 * 메인 메뉴 (WBP_MainMenu)의 C++ 기반 클래스
 * BP에서는 이 클래스를 상속받아 디자인만 구성
 */
UCLASS()
class INDIAN_BAB_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/**
	 * 위젯이 생성될 때 (BP의 EventConstruct) 호출
	 * 버튼 이벤트를 C++ 함수에 바인딩
	 */
	virtual void NativeConstruct() override;

	/** 위젯이 파괴될 때 호출 — 연결성 델리게이트 해제 */
	virtual void NativeDestruct() override;

private:
	//--- BP 위젯 변수 바인딩 ---

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Room_Creation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Room_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Option;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Exit;

	//--- 위젯 설정 프로퍼티 ---

	// '설정' 버튼 클릭 시 열릴 옵션 메뉴 위젯 클래스 (BP에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UOptionMenuWidget> OptionMenuWidgetClass;

	// '룸 생성' 버튼 클릭 시 열릴 레벨 이름 (BP의 클래스 기본값에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URoomCreateWidget> RoomCreateWidgetClass;

	// '룸 참가' 버튼 클릭 시 열릴 레벨 이름 (BP의 클래스 기본값에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URoomJoinWidget> RoomJoinWidgetClass;

	// 인터넷 연결 끊김 시 표시할 오프라인 알림 위젯 클래스 (BP에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> OfflineWidgetClass;

	//--- 위젯 인스턴스 캐시 ---

	UPROPERTY()
	TObjectPtr<URoomCreateWidget> RooomCreateInstance;

	UPROPERTY()
	TObjectPtr<URoomJoinWidget> RooomJoinInstance;

	UPROPERTY()
	TObjectPtr<UOptionMenuWidget> OptionMenuInstance;

	UPROPERTY()
	TObjectPtr<UUserWidget> OfflineWidgetInstance;

	// --- 기타 캐시된 참조 ---
	UPROPERTY()
	TObjectPtr<APlayerController> PlayerControllerRef;

	//--- C++ 이벤트 핸들러 ---

	UFUNCTION()
	void OnRoomCreationClicked();

	UFUNCTION()
	void OnRoomJoinClicked();

	UFUNCTION()
	void OnOptionClicked();

	UFUNCTION()
	void OnExitClicked();

	//--- 연결성 콜백 ---

	void HandleConnectivityLost();
	void HandleConnectivityRestored();

	FDelegateHandle LostHandle;
	FDelegateHandle RestoredHandle;
};
