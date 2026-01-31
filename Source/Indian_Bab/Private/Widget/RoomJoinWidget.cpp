#include "Widget/RoomJoinWidget.h"
#include "Components/Button.h"

void URoomJoinWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	ParentMenu = InParentMenu;
}


void URoomJoinWidget::OnYesClicked()
{

}

void URoomJoinWidget::OnNoClicked()
{
	// 부모 메뉴가 있다면 다시 보이게 함
	if (ParentMenu)
	{
		// 부모 위젯(PauseMenu)으로 돌아가기 위해 GameAndUI 모드로 '복원' 및 포커싱 재설정
		if (PlayerControllerRef)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(ParentMenu->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
	// 자신을 뷰포트에서 제거
	RemoveFromParent();
}

void URoomJoinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 플레이어 컨트롤러 캐시
	PlayerControllerRef = GetOwningPlayer();

	// --- 모든 UI 이벤트에 C++ 함수 바인딩 ---
	// 메인 버튼
	Button_Yes->OnClicked.AddDynamic(this, &URoomJoinWidget::OnYesClicked);
	Button_No->OnClicked.AddDynamic(this, &URoomJoinWidget::OnNoClicked);
}


/**
 * ESC 키 입력을 처리하는 함수
 */
FReply URoomJoinWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 키가 눌렸는지 확인
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		UE_LOG(LogTemp, Log, TEXT("RoomJoinWidget: ESC key pressed."));

		// 그냥 닫습니다
		OnNoClicked();

		// [중요] ESC 키 입력을 '처리했음(Handled)'으로 반환
		// 이렇게 해야 입력이 PlayerController로 전파되어
		// PauseMenu가 닫히는 것을 막을 수 있음
		return FReply::Handled();
	}

	// 다른 키가 눌렸다면 '처리 안 함(Unhandled)'으로 반환
	return FReply::Unhandled();
}


/**
 * '배경' 클릭 시 포커스를 잃지 않도록 처리하는 함수
 */
FReply URoomJoinWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 기본 마우스 로직을 먼저 호출 (예: 버튼 클릭)
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// 팝업이 없고, 플레이어 컨트롤러가 유효하다면
	// (사용자가 이 위젯의 빈 공간(배경)을 클릭했더라도)
	// 이 위젯에게 다시 키보드 포커스를 강제로 설정
	if (PlayerControllerRef)
	{
		//// SWidget에 대한 포인터를 가져옵니다.
		//TSharedPtr<SWidget> SafeWidget = this->TakeWidget();
		//if (SafeWidget.IsValid())
		//{
		//	// "이 위젯에 키보드 포커스를 설정하라"
		//	FSlateApplication::Get().SetKeyboardFocus(SafeWidget);
		//}
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(this->TakeWidget()); // 포커스를 이 위젯으로
		PlayerControllerRef->SetInputMode(InputModeData);
	}

	// FReply::Handled()를 반환
	// 이 클릭을 "처리했음(Handled)"으로 표시하여,
	// 클릭 이벤트가 뷰포트로 "떨어지는(fall-through)" 것을 막음
	return FReply::Handled();
}