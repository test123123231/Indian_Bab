#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "MainMenuWidget.generated.h"


class UButton;
class UOptionMenuWidget;
class URoomCreateWidget;
class URoomJoinWidget;
class USessionSubsystem;


/**
 * 메인 메뉴 (WBP_MainMenu)의 C++ 기반 클래스
 * BP에서는 이 클래스를 상속받아 디자인만 구성
 */
UCLASS()
class INDIAN_BAB_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/**
	 * 위젯이 생성될 때 (BP의 EventConstruct) 호출
	 * 버튼 이벤트를 C++ 함수에 바인딩
	 */
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// SessionErrorWidget의 확인 버튼 BP가 RemoveFromParent 후 호출 — 메인메뉴로 입력 포커스 복원.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefocusSelf();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMainMenuRoot();

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

	UPROPERTY(meta = (BindWidget))
	UWidgetSwitcher* Switcher_MainMenu;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URoomCreateWidget> WBP_CreateRoom;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URoomJoinWidget> WBP_JoinRoom;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOptionMenuWidget> WBP_OptionMenu;

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

	// 매치메이커 실패·데디 PreLogin 거부 등 모든 세션 에러 사유 모달 (BP에서 WBP_SessionErrorNotice 지정)
	// 위젯은 BindWidget으로 TextBlock(Text_Message) 노출 필요.
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> SessionErrorWidgetClass;

	//--- 위젯 인스턴스 캐시 ---

	UPROPERTY()
	TObjectPtr<URoomCreateWidget> RooomCreateInstance;

	UPROPERTY()
	TObjectPtr<URoomJoinWidget> RooomJoinInstance;

	UPROPERTY()
	TObjectPtr<UOptionMenuWidget> OptionMenuInstance;

	UPROPERTY()
	TObjectPtr<UUserWidget> SessionErrorInstance;

	// 현재 메인메뉴 위에 떠있는 자식 모달(RoomCreate/RoomJoin/Option 중 하나).
	// SessionErrorWidget 닫힘 시 RefocusSelf가 이 모달이 살아있으면 그쪽으로 포커스를 돌려준다.
	// 자식이 RemoveFromParent로 사라져도 별도 clear는 안 함 — RefocusSelf에서 IsInViewport로 검증.
	UPROPERTY()
	TObjectPtr<UUserWidget> TopmostChildModal;

	// OnSessionErrorEvent 구독/해제용 캐시
	UPROPERTY()
	TObjectPtr<USessionSubsystem> SessionSubsystem;

	// --- 기타 캐시된 참조 ---
	UPROPERTY()
	TObjectPtr<APlayerController> PlayerControllerRef;

	//--- C++ 이벤트 핸들러 ---

	bool ShouldOpenViewportMenu() const;

	UFUNCTION()
	void OnRoomCreationClicked();

	UFUNCTION()
	void OnRoomJoinClicked();

	UFUNCTION()
	void OnOptionClicked();

	UFUNCTION()
	void OnExitClicked();

	// 매치메이커 사유 / 데디 거부 공용 핸들러 — SessionErrorWidget 모달 표시.
	UFUNCTION()
	void OnSessionError(const FString& Reason);
};
