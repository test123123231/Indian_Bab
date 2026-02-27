#include "GameMode/MainGameMode.h"
#include "PlayerController/MainGamePlayerController.h"

AMainGameMode::AMainGameMode()
{
    PlayerControllerClass = AMainGamePlayerController::StaticClass();
}

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    FString TimeString = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
    UE_LOG(LogTemp, Warning, TEXT("[GM] BeginPlay / Time [%s] / Phase=%d"), *TimeString, (int32)Phase);
}

void AMainGameMode::HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

    int PlayerId = RequestPC->GetPlayerIdSafe();

    const TCHAR* ActionStr = TEXT("UnKown");
    if (Action == EBetAction::Raise)
        ActionStr = TEXT("Raise");
    else if (Action == EBetAction::CheckCall)
        ActionStr = TEXT("CheckCall");
    else if (Action == EBetAction::Fold)
        ActionStr = TEXT("Fold");

    UE_LOG(LogTemp, Warning, TEXT("[GM][Input] PC = %d, Action = %s"),PlayerId, ActionStr);
}