#include "Widget/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "PlayerController/MainMenuPlayerController.h"
#include "Widget/OptionMenuWidget.h"
#include "Widget/RoomCreateWidget.h"
#include "Widget/RoomJoinWidget.h"


UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// SetInputModeUIOnly 포커스 대상이 되므로 기본 focusable 보장 — BP의 Is Focusable 체크 누락 방지
	SetIsFocusable(true);
}

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

	URoomCreateWidget* EmbeddedCreateRoom = WBP_CreateRoom ? WBP_CreateRoom.Get() : Cast<URoomCreateWidget>(GetWidgetFromName(TEXT("WBP_CreateRoom")));
	URoomJoinWidget* EmbeddedJoinRoom = WBP_JoinRoom ? WBP_JoinRoom.Get() : Cast<URoomJoinWidget>(GetWidgetFromName(TEXT("WBP_JoinRoom")));
	UOptionMenuWidget* EmbeddedOptionMenu = WBP_OptionMenu ? WBP_OptionMenu.Get() : Cast<UOptionMenuWidget>(GetWidgetFromName(TEXT("WBP_OptionMenu")));

	if (EmbeddedCreateRoom)
	{
		EmbeddedCreateRoom->SetParentMenu(this);
	}
	if (EmbeddedJoinRoom)
	{
		EmbeddedJoinRoom->SetParentMenu(this);
	}
	if (EmbeddedOptionMenu)
	{
		EmbeddedOptionMenu->SetParentMenu(this);
	}

	PlayerControllerRef = GetOwningPlayer();

	// 모든 세션/매치메이커/데디 거부 에러 단일 채널 구독 — 자식 모달과 동일 패턴.
	if (UGameInstance* GI = GetGameInstance())
	{
		SessionSubsystem = GI->GetSubsystem<USessionSubsystem>();
		if (SessionSubsystem)
		{
			SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &UMainMenuWidget::OnSessionError);
			SessionSubsystem->OnSessionErrorEvent.AddDynamic(this, &UMainMenuWidget::OnSessionError);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[MainMenu Bind Check] Create=%s Join=%s Option=%s Exit=%s Switcher=%s"),
	*GetNameSafe(Button_Room_Creation),
	*GetNameSafe(Button_Room_Join),
	*GetNameSafe(Button_Option),
	*GetNameSafe(Button_Exit),
	*GetNameSafe(Switcher_MainMenu));
}


void UMainMenuWidget::NativeDestruct()
{
	// 자기 자신의 BindWidget UObject들 — 일관성 위해 정리 (현재 1회 생성이라 누적 우려는 없음).
	if (Button_Room_Creation) Button_Room_Creation->OnClicked.RemoveAll(this);
	if (Button_Room_Join)     Button_Room_Join->OnClicked.RemoveAll(this);
	if (Button_Option)        Button_Option->OnClicked.RemoveAll(this);
	if (Button_Exit)          Button_Exit->OnClicked.RemoveAll(this);

	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &UMainMenuWidget::OnSessionError);
	}

	Super::NativeDestruct();
}


// 모달이 자기 자신을 닫은 뒤 호출 — 떠있는 자식 모달이 있으면 그쪽, 없으면 자신으로 포커스 복원.
// 자식 모달의 lifecycle은 자체 관리이므로 IsInViewport로 살아있는지만 검증.
void UMainMenuWidget::RefocusSelf()
{
	if (!PlayerControllerRef) return;

	UUserWidget* FocusTarget = this;
	if (TopmostChildModal && TopmostChildModal->IsInViewport())
	{
		FocusTarget = TopmostChildModal;
	}

	if (AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PlayerControllerRef))
	{
		MainMenuPC->ApplyMenuInputMode(FocusTarget);
		return;
	}

	FInputModeUIOnly InputModeData;
	InputModeData.SetWidgetToFocus(FocusTarget->TakeWidget());
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerControllerRef->SetInputMode(InputModeData);
}


// 매치메이커 실패 / 데디 거부 공용 모달.
// 위젯 BP는 BindWidget으로 "Text_Message" TextBlock 노출 시 사유 주입됨.
void UMainMenuWidget::ShowMainMenuRoot()
{
	if (Switcher_MainMenu)
	{
		Switcher_MainMenu->SetActiveWidgetIndex(0);
	}

	TopmostChildModal = nullptr;
	RefocusSelf();
}

void UMainMenuWidget::OnSessionError(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("MainMenuWidget: session error — %s"), *Reason);

	if (!SessionErrorWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuWidget: SessionErrorWidgetClass 미지정 — 사유 로그만 남깁니다."));
		return;
	}

	if (!SessionErrorInstance)
	{
		SessionErrorInstance = CreateWidget<UUserWidget>(this, SessionErrorWidgetClass);
		if (SessionErrorInstance)
		{
			SessionErrorInstance->SetIsFocusable(true);
		}
	}

	if (!SessionErrorInstance) return;

	// 위젯 내부 TextBlock 주입 — BindWidget 미적용 시 nullptr이라 nullptr 체크 필수
	if (UTextBlock* Msg = Cast<UTextBlock>(
		SessionErrorInstance->GetWidgetFromName(TEXT("Text_Message"))))
	{
		Msg->SetText(FText::FromString(Reason));
	}

	if (!SessionErrorInstance->IsInViewport())
	{
		SessionErrorInstance->AddToViewport(100);

		// VR 모드에서 SetInputMode(UIOnly)는 WidgetInteractionComponent 시뮬 키 라우팅을 깨뜨림 — Mouse 모드만 적용
		if (PlayerControllerRef)
		{
			const AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PlayerControllerRef);
			if (!MainMenuPC || MainMenuPC->IsMouseMenuInputMode())
			{
				FInputModeUIOnly InputModeData;
				InputModeData.SetWidgetToFocus(SessionErrorInstance->TakeWidget());
				InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PlayerControllerRef->SetInputMode(InputModeData);
			}
		}
	}
}


// '룸 생성' 버튼 로직
bool UMainMenuWidget::ShouldOpenViewportMenu() const
{
	APlayerController* PC = PlayerControllerRef ? PlayerControllerRef.Get() : GetOwningPlayer();
	const AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PC);
	return MainMenuPC && MainMenuPC->IsMouseMenuInputMode();
}

void UMainMenuWidget::OnRoomCreationClicked()
{
	if (!ShouldOpenViewportMenu())
	{
		UE_LOG(LogTemp, Warning, TEXT("VR Mode: Switch to Create Room"));

		if (Switcher_MainMenu)
		{
			Switcher_MainMenu->SetActiveWidgetIndex(1);
		}
		return;
	}

	if (!RoomCreateWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseRoomCreateWidget: RoomCreateWidgetClass 설정되지 않았습니다!"));
		return;
	}

	if (!RooomCreateInstance)
	{
		RooomCreateInstance = CreateWidget<URoomCreateWidget>(this, RoomCreateWidgetClass);
		if (!RooomCreateInstance) return;

		RooomCreateInstance->SetParentMenu(this);
	}

	if (RooomCreateInstance && !RooomCreateInstance->IsInViewport())
	{
		RooomCreateInstance->AddToViewport();
		TopmostChildModal = RooomCreateInstance;

		if (PlayerControllerRef)
		{
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
	UE_LOG(LogTemp, Warning, TEXT("OnRoomJoinClicked called"));
	if (!ShouldOpenViewportMenu())
	{
		UE_LOG(LogTemp, Warning, TEXT("VR Mode: Switch to Join Room"));

		if (Switcher_MainMenu)
		{
			Switcher_MainMenu->SetActiveWidgetIndex(2);
		}
		return;
	}

	if (!RoomJoinWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseRoomJoinWidget: RoomJoinWidgetClass 설정되지 않았습니다!"));
		return;
	}

	if (!RooomJoinInstance)
	{
		RooomJoinInstance = CreateWidget<URoomJoinWidget>(this, RoomJoinWidgetClass);
		if (!RooomJoinInstance) return;

		RooomJoinInstance->SetParentMenu(this);
	}

	if (RooomJoinInstance && !RooomJoinInstance->IsInViewport())
	{
		RooomJoinInstance->AddToViewport();
		TopmostChildModal = RooomJoinInstance;

		if (PlayerControllerRef)
		{
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
	UE_LOG(LogTemp, Warning, TEXT("OnOptionClicked called"));
	if (!ShouldOpenViewportMenu())
	{
		UE_LOG(LogTemp, Warning, TEXT("VR Mode: Switch to Option Menu"));

		if (Switcher_MainMenu)
		{
			Switcher_MainMenu->SetActiveWidgetIndex(3);
		}
		return;
	}

	if (!OptionMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseMenuWidget: OptionMenuWidgetClass가 설정되지 않았습니다!"));
		return;
	}

	if (!OptionMenuInstance)
	{
		OptionMenuInstance = CreateWidget<UOptionMenuWidget>(this, OptionMenuWidgetClass);
		if (!OptionMenuInstance) return;

		OptionMenuInstance->SetParentMenu(this);
	}

	if (OptionMenuInstance && !OptionMenuInstance->IsInViewport())
	{
		OptionMenuInstance->AddToViewport();
		TopmostChildModal = OptionMenuInstance;

		if (PlayerControllerRef)
		{
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
