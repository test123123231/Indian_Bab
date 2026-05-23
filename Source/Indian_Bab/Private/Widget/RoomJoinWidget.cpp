#include "Widget/RoomJoinWidget.h"
#include "Widget/RoomListEntryData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "Kismet/GameplayStatics.h"


void URoomJoinWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	ParentMenu = InParentMenu;
}


void URoomJoinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlayerControllerRef = GetOwningPlayer();

	if (GetGameInstance())
	{
		SessionSubsystem = GetGameInstance()->GetSubsystem<USessionSubsystem>();
	}

	if (SessionSubsystem)
	{
		SessionSubsystem->OnJoinSessionCompleteEvent.RemoveDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);
		SessionSubsystem->OnJoinSessionCompleteEvent.AddDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);

		SessionSubsystem->OnRoomsFoundEvent.RemoveDynamic(this, &URoomJoinWidget::OnRoomsFound);
		SessionSubsystem->OnRoomsFoundEvent.AddDynamic(this, &URoomJoinWidget::OnRoomsFound);
	}

	if (Button_Yes)
	{
		Button_Yes->OnClicked.AddDynamic(this, &URoomJoinWidget::OnYesClicked);
		// 초기엔 선택 없음 → 참가 비활성
		Button_Yes->SetIsEnabled(false);
	}
	if (Button_No)
	{
		Button_No->OnClicked.AddDynamic(this, &URoomJoinWidget::OnNoClicked);
	}
	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddDynamic(this, &URoomJoinWidget::OnRefreshClicked);
	}

	if (ListView_Rooms)
	{
		ListView_Rooms->OnItemClicked().AddUObject(this, &URoomJoinWidget::OnRoomItemClicked);
	}

	// 진입 시 자동 1회 검색
	OnRefreshClicked();
}


void URoomJoinWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (SessionSubsystem)
	{
		SessionSubsystem->OnJoinSessionCompleteEvent.RemoveDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);
		SessionSubsystem->OnRoomsFoundEvent.RemoveDynamic(this, &URoomJoinWidget::OnRoomsFound);
	}
}


FReply URoomJoinWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnNoClicked();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply URoomJoinWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	if (PlayerControllerRef)
	{
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(this->TakeWidget());
		PlayerControllerRef->SetInputMode(InputModeData);
	}

	return FReply::Handled();
}


void URoomJoinWidget::OnRefreshClicked()
{
	if (!SessionSubsystem)
	{
		SetStatusText(TEXT("Session unavailable."), FLinearColor::Red);
		return;
	}

	SelectedSearchIndex = INDEX_NONE;
	if (Button_Yes) Button_Yes->SetIsEnabled(false);
	if (ListView_Rooms) ListView_Rooms->ClearListItems();

	SetStatusText(TEXT("Searching rooms..."), FLinearColor::White);

	if (Button_Refresh) Button_Refresh->SetIsEnabled(false);
	SessionSubsystem->FindRooms();
}


void URoomJoinWidget::OnRoomsFound(bool bWasSuccessful, const TArray<FRoomListEntry>& Rooms)
{
	if (Button_Refresh) Button_Refresh->SetIsEnabled(true);

	if (!ListView_Rooms)
	{
		return;
	}

	ListView_Rooms->ClearListItems();

	if (!bWasSuccessful)
	{
		SetStatusText(TEXT("Search failed."), FLinearColor::Red);
		return;
	}

	if (Rooms.Num() == 0)
	{
		SetStatusText(TEXT("No rooms found."), FLinearColor::Gray);
		return;
	}

	for (const FRoomListEntry& E : Rooms)
	{
		URoomListEntryData* Data = NewObject<URoomListEntryData>(this);
		Data->Entry = E;
		Data->bJoinable = !E.bGameStarted;
		ListView_Rooms->AddItem(Data);
	}

	SetStatusText(FString::Printf(TEXT("%d room(s) found."), Rooms.Num()), FLinearColor::White);
}


void URoomJoinWidget::OnRoomItemClicked(UObject* Item)
{
	URoomListEntryData* Data = Cast<URoomListEntryData>(Item);
	if (Data == nullptr || !Data->bJoinable)
	{
		SelectedSearchIndex = INDEX_NONE;
		if (Button_Yes) Button_Yes->SetIsEnabled(false);
		return;
	}

	SelectedSearchIndex = Data->Entry.SearchResultIndex;
	if (Button_Yes) Button_Yes->SetIsEnabled(true);
}


void URoomJoinWidget::OnYesClicked()
{
	if (!SessionSubsystem || SelectedSearchIndex == INDEX_NONE)
	{
		SetStatusText(TEXT("Select a room first."), FLinearColor::Red);
		return;
	}

	Button_Yes->SetIsEnabled(false);
	if (Button_No) Button_No->SetIsEnabled(false);
	if (Button_Refresh) Button_Refresh->SetIsEnabled(false);

	SetStatusText(TEXT("Joining..."), FLinearColor::White);

	SessionSubsystem->JoinRoomByIndex(SelectedSearchIndex);
}


void URoomJoinWidget::OnJoinSessionComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		SetStatusText(TEXT("Joined. Traveling..."), FLinearColor::Green);
		// 레벨 이동은 SessionSubsystem 내부에서 ClientTravel 처리됨
		return;
	}

	// 실패 → UI 복구
	if (Button_No) Button_No->SetIsEnabled(true);
	if (Button_Refresh) Button_Refresh->SetIsEnabled(true);
	// Yes 버튼은 선택이 살아있어야 다시 활성
	if (Button_Yes) Button_Yes->SetIsEnabled(SelectedSearchIndex != INDEX_NONE);

	SetStatusText(TEXT("Failed to join."), FLinearColor::Red);
}


void URoomJoinWidget::OnNoClicked()
{
	if (ParentMenu && PlayerControllerRef)
	{
		FInputModeGameAndUI InputModeData;
		InputModeData.SetWidgetToFocus(ParentMenu->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
		PlayerControllerRef->SetInputMode(InputModeData);
	}
	RemoveFromParent();
}


void URoomJoinWidget::SetStatusText(const FString& Msg, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Msg));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
		return;
	}
	// fallback: 상태 전용 슬롯이 없으면 TitleText 재사용
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Msg));
		TitleText->SetColorAndOpacity(FSlateColor(Color));
	}
}
