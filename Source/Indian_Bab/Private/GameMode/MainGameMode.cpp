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