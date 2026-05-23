#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "RoomJoinWidget.generated.h"


class UButton;
class UTextBlock;
class UListView;
class USessionSubsystem;
class URoomListEntryData;


UCLASS()
class INDIAN_BAB_API URoomJoinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetParentMenu(UUserWidget* InParentMenu);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY()
	TObjectPtr<USessionSubsystem> SessionSubsystem;

	// Button_Yes = 참가, Button_No = 닫기, Button_Refresh = 검색 새로고침
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Yes;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_No;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Refresh;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	// 상태 메시지 (검색 중/실패/방 없음) — 옵셔널
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> ListView_Rooms;

	UPROPERTY()
	TObjectPtr<APlayerController> PlayerControllerRef;
	UPROPERTY()
	TObjectPtr<UUserWidget> ParentMenu;

	// 현재 선택된 검색결과 인덱스 (없으면 INDEX_NONE)
	int32 SelectedSearchIndex = INDEX_NONE;

	UFUNCTION() void OnYesClicked();
	UFUNCTION() void OnNoClicked();
	UFUNCTION() void OnRefreshClicked();

	UFUNCTION() void OnRoomsFound(bool bWasSuccessful, const TArray<FRoomListEntry>& Rooms);
	UFUNCTION() void OnRoomItemClicked(UObject* Item);
	UFUNCTION() void OnJoinSessionComplete(bool bWasSuccessful);

	void SetStatusText(const FString& Msg, const FLinearColor& Color);
};
