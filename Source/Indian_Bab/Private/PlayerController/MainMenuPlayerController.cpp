#include "PlayerController/MainMenuPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Widget/MainMenuWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	OpenMainMenu();
}

void AMainMenuPlayerController::OpenMainMenu()
{
	// MainMenuWidgetClass 변수에 유효한 위젯 클래스가 할당되었는지 확인합니다.
	if (MainMenuWidgetClass)
	{
		// 위젯 인스턴스를 생성하고 MainMenuWidgetInstance 변수에 저장합니다.
		MainMenuWidgetInstance = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);

		// 위젯 생성이 성공했는지 다시 한번 확인합니다.
		if (MainMenuWidgetInstance)
		{
			// 생성된 위젯을 뷰포트에 추가하여 화면에 표시합니다.
			MainMenuWidgetInstance->AddToViewport();

			// 입력 모드를 게임 및 UI 겸용으로 변경
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(MainMenuWidgetInstance->TakeWidget()); // 포커스를 위젯으로
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);

			bShowMouseCursor = true; // 마우스 커서 보이기
		}
	}
}