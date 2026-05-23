#include "PlayerController/MainMenuPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Widget/MainMenuWidget.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
		return;

	OpenMainMenu();

	// 연결성 구독 + 폴링 시작 — 메인메뉴 PC 살아있는 동안만 활성
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			LostHandle = Connectivity->OnConnectivityLost.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityLost);
			RestoredHandle = Connectivity->OnConnectivityRestored.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityRestored);

			Connectivity->StartPolling();
		}

		// 모든 세션/매치메이커/데디 거부 에러 단일 채널 구독
		if (USessionSubsystem* Session = GI->GetSubsystem<USessionSubsystem>())
		{
			Session->OnSessionErrorEvent.AddDynamic(
				this, &AMainMenuPlayerController::HandleSessionError);
		}
	}
}

void AMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 델리게이트 해제 + 폴링 정지 — PC 파괴 시 dangling 핸들 방지
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			Connectivity->OnConnectivityLost.Remove(LostHandle);
			Connectivity->OnConnectivityRestored.Remove(RestoredHandle);
			Connectivity->StopPolling();
		}

		if (USessionSubsystem* Session = GI->GetSubsystem<USessionSubsystem>())
		{
			Session->OnSessionErrorEvent.RemoveDynamic(
				this, &AMainMenuPlayerController::HandleSessionError);
		}
	}

	Super::EndPlay(EndPlayReason);
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
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(MainMenuWidgetInstance->TakeWidget()); // 포커스를 위젯으로
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);

			bShowMouseCursor = true; // 마우스 커서 보이기
		}
	}
}

// 인터넷 끊김 (Grace 2초 경과 후 호출)
void AMainMenuPlayerController::HandleConnectivityLost()
{
	if (!OfflineWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: OfflineWidgetClass 설정되지 않았습니다!"));
		return;
	}

	if (!OfflineWidgetInstance)
	{
		OfflineWidgetInstance = CreateWidget<UUserWidget>(this, OfflineWidgetClass);
		if (OfflineWidgetInstance)
		{
			OfflineWidgetInstance->SetIsFocusable(true);
		}
	}

	if (OfflineWidgetInstance && !OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->AddToViewport(100); // ZOrder 높게 — 메인메뉴 위에 표시

		// 오프라인 모달에만 포커스 — 뒤 버튼 입력 차단
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(OfflineWidgetInstance->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);
	}
}

// 모달이 자기 자신을 닫은 뒤 호출 — MainMenuWidget으로 입력 포커스 복원.
// 모달 lifecycle(RemoveFromParent)은 호출 측 BP 책임. 여기서는 포커스만 다룬다.
void AMainMenuPlayerController::RefocusMainMenu()
{
	if (!MainMenuWidgetInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: RefocusMainMenu — MainMenuWidget 없음, 입력 모드 미변경."));
		return;
	}

	FInputModeUIOnly InputModeData;
	InputModeData.SetWidgetToFocus(MainMenuWidgetInstance->TakeWidget());
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputModeData);
}


// 매치메이커 실패 / 데디 거부 공용 모달.
// 위젯 BP는 "MessageText"라는 TextBlock을 BindWidget으로 노출해야 사유가 주입됨.
// 위젯 클래스가 미지정이면 로그만 남기고 무시 — 게임 흐름은 막지 않음(메인메뉴 잔류).
void AMainMenuPlayerController::HandleSessionError(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: session error — %s"), *Reason);

	if (!SessionErrorWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: SessionErrorWidgetClass 미지정 — 사유 로그만 남깁니다."));
		return;
	}

	if (!SessionErrorWidgetInstance)
	{
		SessionErrorWidgetInstance = CreateWidget<UUserWidget>(this, SessionErrorWidgetClass);
		if (SessionErrorWidgetInstance)
		{
			SessionErrorWidgetInstance->SetIsFocusable(true);
		}
	}

	if (!SessionErrorWidgetInstance) return;

	// 위젯 내부 TextBlock 주입 — BindWidget 미적용 시 nullptr이라 nullptr 체크 필수
	if (UTextBlock* Msg = Cast<UTextBlock>(
		SessionErrorWidgetInstance->GetWidgetFromName(TEXT("Text_Message"))))
	{
		Msg->SetText(FText::FromString(Reason));
	}

	if (!SessionErrorWidgetInstance->IsInViewport())
	{
		SessionErrorWidgetInstance->AddToViewport(100);

		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(SessionErrorWidgetInstance->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);
	}
}


// 연결 복구 (Lost 카운트다운 중 회복된 경우)
void AMainMenuPlayerController::HandleConnectivityRestored()
{
	if (OfflineWidgetInstance && OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->RemoveFromParent();
		RefocusMainMenu();
	}
}
