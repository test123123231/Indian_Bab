#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "RoomListEntryData.generated.h"


/**
 * ListView 한 행에 바인딩되는 데이터 객체.
 * UListView는 UObject만 ItemSource로 받기 때문에 FRoomListEntry를 래핑한다.
 */
UCLASS(BlueprintType)
class INDIAN_BAB_API URoomListEntryData : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Session")
    FRoomListEntry Entry;

    // bGameStarted=true 인 방은 위젯에서 회색·클릭 비활성 (JoinRoomByIndex 가드 백스톱과 별개)
    UPROPERTY(BlueprintReadOnly, Category = "Session")
    bool bJoinable = true;
};
