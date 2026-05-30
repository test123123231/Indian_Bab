#include "Widget/RoomListEntryWidget.h"
#include "Widget/RoomListEntryData.h"
#include "Components/TextBlock.h"


void URoomListEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

    URoomListEntryData* Data = Cast<URoomListEntryData>(ListItemObject);
    if (Data == nullptr)
    {
        return;
    }

    const FRoomListEntry& E = Data->Entry;

    if (Text_Host)
    {
        Text_Host->SetText(FText::FromString(E.HostName.IsEmpty() ? TEXT("(unknown host)") : E.HostName));
    }
    if (Text_Players)
    {
        Text_Players->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), E.CurrentPlayers, E.MaxPlayers)));
    }
    if (Text_Status)
    {
        Text_Status->SetText(FText::FromString(E.bGameStarted ? TEXT("in-game") : TEXT("in-lobby")));
    }
    if (Text_FriendsOnly)
    {
        Text_FriendsOnly->SetText(FText::FromString(E.bFriendsOnly ? TEXT("Friends") : TEXT("")));
    }

    // GameStarted 인 행은 위젯 자체를 disable → 클릭/선택 불가
    SetIsEnabled(Data->bJoinable);
}


void URoomListEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
    IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
    BP_OnSelectionChanged(bIsSelected);
}
