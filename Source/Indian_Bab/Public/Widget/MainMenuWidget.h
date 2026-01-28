#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"


class UButton;
class UOptionMenuWidget;


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

	// '시작하기' 버튼 클릭 시 열릴 레벨 이름 (BP의 클래스 기본값에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	FName GameLevelName = FName(TEXT("AreaKeeper"));

	// '설정' 버튼 클릭 시 열릴 옵션 메뉴 위젯 클래스 (BP에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Config", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UOptionMenuWidget> OptionMenuWidgetClass;

	// '설정' 메뉴 인스턴스 캐시
	UPROPERTY()
	TObjectPtr<UOptionMenuWidget> OptionMenuInstance;

	// --- 기타 캐시된 참조 ---
	UPROPERTY()
	TObjectPtr<APlayerController> PlayerControllerRef;

	//--- C++ 이벤트 핸들러 ---

	// '룸 생성' 버튼 클릭 시
	UFUNCTION()
	void OnRoomCreationClicked();

	// '룸 참가' 버튼 클릭 시
	UFUNCTION()
	void OnRoomJoinClicked();

	// '설정' 버튼 클릭 시
	UFUNCTION()
	void OnOptionClicked();

	// '종료' 버튼 클릭 시
	UFUNCTION()
	void OnExitClicked();

	
};
