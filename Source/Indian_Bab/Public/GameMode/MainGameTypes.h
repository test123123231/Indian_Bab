#pragma once

#include "CoreMinimal.h"
#include "MainGameTypes.generated.h"

UENUM(BlueprintType)
enum class EBetAction : uint8
{
    CheckCall,
    Raise,
    Fold
};

UENUM(BlueprintType)
enum class EMainGamePhase : uint8
{
    WaitingPlayers,
    Betting,
    Resolve,
    EndRound
};