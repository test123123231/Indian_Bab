#include "PlayerController/MainGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "Widget/MainGameWidget.h"

void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if(!IsLocalController()) return;
    CreateMainGameWidget();
    EnterUIMode();

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsys) return;

    if (DefaultMappingContext)
    {
        Subsys->AddMappingContext(DefaultMappingContext, 0);
    }
}

void AMainGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
{
    if (IA_Raise) EIC->BindAction(IA_Raise, ETriggerEvent::Started, this, &AMainGamePlayerController::RequestRaise);
    if (IA_CheckCall) EIC->BindAction(IA_CheckCall, ETriggerEvent::Started, this, &AMainGamePlayerController::RequestCheckCall);
    if (IA_Fold) EIC->BindAction(IA_Fold, ETriggerEvent::Started, this, &AMainGamePlayerController::RequestFold);
    if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLook);
    if (IA_RMB)
    {
        EIC->BindAction(IA_RMB, ETriggerEvent::Started,   this, &AMainGamePlayerController::OnRMBPressed);
        EIC->BindAction(IA_RMB, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnRMBReleased);
    }
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

void AMainGamePlayerController::OnRMBPressed(const FInputActionValue& Value)
{
    bRMBHeld = true;
    EnterCameraMode();
}

void AMainGamePlayerController::OnRMBReleased(const FInputActionValue& Value)
{
    bRMBHeld = false;
    EnterUIMode();
}

void AMainGamePlayerController::EnterUIMode()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);

    SetIgnoreLookInput(true);
}

void AMainGamePlayerController::EnterCameraMode()
{
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;

    FInputModeGameOnly Mode;
    SetInputMode(Mode);

    SetIgnoreLookInput(false);
}

void AMainGamePlayerController::OnLook(const FInputActionValue& Value)
{
    if (!bRMBHeld) return;

    const FVector2D LookAxis = Value.Get<FVector2D>();

    AddYawInput(LookAxis.X);
    AddPitchInput(-LookAxis.Y);
}

void AMainGamePlayerController::RequestRaise()
{
    UE_LOG(LogTemp, Display, TEXT("RequestRaise"));
}

void AMainGamePlayerController::RequestCheckCall()
{
    UE_LOG(LogTemp, Display, TEXT("RequestCheckCall"));
}

void AMainGamePlayerController::RequestFold()
{
    UE_LOG(LogTemp, Display, TEXT("RequestFold"));
}
