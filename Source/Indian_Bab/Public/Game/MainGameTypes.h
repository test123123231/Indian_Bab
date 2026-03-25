#pragma once

#include "CoreMinimal.h"
#include "MainGameTypes.generated.h"

UENUM(BlueprintType)
enum class EBetAction : uint8
{
    None,
    CheckCall,
    Raise,
    Fold
};

// 총을 집어드는 이유 (ABP 스테이트 머신 트랜지션 판별용)
UENUM(BlueprintType)
enum class EGunHoldReason : uint8
{
    None,   // 총을 들지 않은 상태
    Fold,   // 폴드 시 자기 머리를 겨냥
    Win     // 승리 시 총을 겨냥
};

// UENUM(BlueprintType)
// enum class EMainGamePhase : uint8
// {
//     WaitingPlayers,
//     Betting,
//     Resolve,
//     EndRound
// };