#include "Widget/MainMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Widget/OptionMenuWidget.h"
#include "Widget/RoomCreateWidget.h"
#include "Widget/RoomJoinWidget.h"


void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// C++ 함수와 BP 버튼의 OnClicked 이벤트를 바인딩
	if (Button_Room_Creation)
	{
		Button_Room_Creation->OnClicked.AddDynamic(this, &UMainMenuWidget::OnRoomCreationClicked);
	}
	if (Button_Room_Join)
	{
		Button_Room_Join->OnClicked.AddDynamic(this, &UMainMenuWidget::OnRoomJoinClicked);
	}
	if (Button_Option)
	{
		Button_Option->OnClicked.AddDynamic(this, &UMainMenuWidget::OnOptionClicked);
	}
	if (Button_Exit)
	{
		Button_Exit->OnClicked.AddDynamic(this, &UMainMenuWidget::OnExitClicked);
	}

	PlayerControllerRef = GetOwningPlayer();
}


// '룸 생성' 버튼 로직
void UMainMenuWidget::OnRoomCreationClicked()
{
	if (!RoomCreateWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseRoomCreateWidget: RoomCreateWidgetClass 설정되지 않았습니다!"));
		return;
	}
	// 생성 메뉴 '인스턴스' 생성
	if (!RooomCreateInstance)
	{
		RooomCreateInstance = CreateWidget<URoomCreateWidget>(this, RoomCreateWidgetClass);
		if (!RooomCreateInstance) return; // 생성 실패 시 중단

		// 룸 생성에 부모(자신)를 알려줌
		RooomCreateInstance->SetParentMenu(this);
	}

	// 뷰포트에 추가 및 포커스 설정
	if (RooomCreateInstance && !RooomCreateInstance->IsInViewport())
	{
		RooomCreateInstance->AddToViewport();

		if (PlayerControllerRef)
		{
			// (GameAndUI) 옵션 메뉴는 ESC 키를 사용하므로 포커스 설정
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(RooomCreateInstance->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
}


// '룸 참가' 버튼 로직
void UMainMenuWidget::OnRoomJoinClicked()
{
	if (!RoomJoinWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseRoomJoinWidget: RoomJoinWidgetClass 설정되지 않았습니다!"));
		return;
	}
	// 참가 메뉴 '인스턴스' 생성
	if (!RooomJoinInstance)
	{
		RooomJoinInstance = CreateWidget<URoomJoinWidget>(this, RoomJoinWidgetClass);
		if (!RooomJoinInstance) return; // 생성 실패 시 중단

		// 룸 참가에 부모(자신)를 알려줌
		RooomJoinInstance->SetParentMenu(this);
	}

	// 뷰포트에 추가 및 포커스 설정
	if (RooomJoinInstance && !RooomJoinInstance->IsInViewport())
	{
		RooomJoinInstance->AddToViewport();

		if (PlayerControllerRef)
		{
			// (GameAndUI) 옵션 메뉴는 ESC 키를 사용하므로 포커스 설정
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(RooomJoinInstance->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
}


// '설정' 버튼 로직
void UMainMenuWidget::OnOptionClicked()
{
	if (!OptionMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseMenuWidget: OptionMenuWidgetClass가 설정되지 않았습니다!"));
		return;
	}

	// 옵션 메뉴 '인스턴스' 생성
	if (!OptionMenuInstance)
	{
		OptionMenuInstance = CreateWidget<UOptionMenuWidget>(this, OptionMenuWidgetClass);
		if (!OptionMenuInstance) return; // 생성 실패 시 중단

		// 옵션 메뉴에 부모(자신)를 알려줌
		OptionMenuInstance->SetParentMenu(this);
	}

	// 뷰포트에 추가 및 포커스 설정
	if (OptionMenuInstance && !OptionMenuInstance->IsInViewport())
	{
		OptionMenuInstance->AddToViewport();

		if (PlayerControllerRef)
		{
			// (GameAndUI) 옵션 메뉴는 ESC 키를 사용하므로 포커스 설정
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(OptionMenuInstance->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
}


// '종료' 버튼 로직
void UMainMenuWidget::OnExitClicked()
{
	// (기술서) 게임 애플리케이션을 종료
	APlayerController* PC = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, true);
}
