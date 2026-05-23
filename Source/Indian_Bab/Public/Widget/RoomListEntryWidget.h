#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "RoomListEntryWidget.generated.h"


class UTextBlock;


/**
 * UListView의 행 템플릿. WBP_RoomListEntry가 이 클래스를 부모로 갖는다.
 * ListView가 행을 그릴 때 NativeOnListItemObjectSet으로 URoomListEntryData를 주입한다.
 */
UCLASS()
class INDIAN_BAB_API URoomListEntryWidget : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

protected:
    // IUserObjectListEntry
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
    virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

    // BP에서 오버라이드해 선택 시 Border 색/테두리 등 시각 갱신
    UFUNCTION(BlueprintImplementableEvent, Category = "Session")
    void BP_OnSelectionChanged(bool bIsSelected);

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Host;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Players;

    // "in-lobby" / "in-game" 표시. BP에서 색상 분기 가능.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Status;

    // FriendsOnly 표식 텍스트 (옵셔널 — BindWidgetOptional)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_FriendsOnly;
};
