#include "PlayerController/MainGamePlayerController.h"
#include "Widget/MainGameWidget.h"

void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalPlayerController())
    {
        CreateMainGameWidget();
    }
}

void AMainGamePlayerController::CreateMainGameWidget()
{
    if (!MainGameWidgetClass)
    {
        return;
    }
    
    if (!MainGameWidgetInstance)
    {
        MainGameWidgetInstance = CreateWidget<UMainGameWidget>(this, MainGameWidgetClass);
    }

    if (MainGameWidgetInstance && !MainGameWidgetInstance->IsInViewport())
    {
        MainGameWidgetInstance->AddToViewport();
    }
}

