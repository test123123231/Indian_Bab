#include "PlayerController/MainGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "Widget/MainGameWidget.h"
#include "Character/LobbyCameraManager.h"


AMainGamePlayerController::AMainGamePlayerController()
{
	PlayerCameraManagerClass = ALobbyCameraManager::StaticClass();
}


void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if(!IsLocalPlayerController()) 
        return;

    CreateMainGameWidget();
    EnterCameraMode();

	ApplyLobbyMappingContext();

    if (USettingSubsystem* SettingSS = GetGameInstance()->GetSubsystem<USettingSubsystem>())
    {
        LookSensitivity = SettingSS->GetMouseSensitivity();
    }

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}


void AMainGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

	if (!IsLocalPlayerController()) 
        return;

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_MainGameLook)
        {
            EnhancedInput->BindAction(IA_MainGameLook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnMainGameLook);
        }
        if (IA_MainGameRMB)
        {
            EnhancedInput->BindAction(IA_MainGameRMB, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameRMBPressed);
            EnhancedInput->BindAction(IA_MainGameRMB, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnMainGameRMBReleased);
        }
        if (IA_MainGameCheckCall)
        {
            EnhancedInput->BindAction(IA_MainGameCheckCall, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnMainGameCheckCall);
        }
        if (IA_MainGameFold)
        {
            EnhancedInput->BindAction(IA_MainGameFold, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnMainGameFold);
        }
        if (IA_MainGameRaise)
        {
            EnhancedInput->BindAction(IA_MainGameRaise, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnMainGameRaise);
        }

        if (IA_LobbyMove)
        {
            EnhancedInput->BindAction(IA_LobbyMove, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyMove);
        }

        if (IA_LobbyLook)
        {
            EnhancedInput->BindAction(IA_LobbyLook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyLook);
        }
    }
}


void AMainGamePlayerController::ApplyLobbyMappingContext()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsys) return;

    // 나중에 게임 모드 별로 바꾸도록 변경 필요
    if (Subsys->HasMappingContext(MainGameMappingContext))
    {
        Subsys->RemoveMappingContext(MainGameMappingContext);
    }

    if (LobbyMappingContext)
    {
        Subsys->AddMappingContext(LobbyMappingContext, 0);
    }
}


void AMainGamePlayerController::ApplyMainGameMappingContext()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsys) return;

    if (Subsys->HasMappingContext(LobbyMappingContext))
    {
        Subsys->RemoveMappingContext(LobbyMappingContext);
	}

    if (MainGameMappingContext)
    {
        Subsys->AddMappingContext(MainGameMappingContext, 0);
    }
}


void AMainGamePlayerController::CreateMainGameWidget()
{
    if (!MainGameWidgetClass) return;
    
    if (!MainGameWidgetInstance)
    {
        MainGameWidgetInstance = CreateWidget<UMainGameWidget>(this, MainGameWidgetClass);
    }

    if (MainGameWidgetInstance && !MainGameWidgetInstance->IsInViewport())
    {
        MainGameWidgetInstance->AddToViewport();
    }
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


void AMainGamePlayerController::OnMainGameLook(const FInputActionValue& Value)
{
    if (!bRMBHeld) return;

    const FVector2D LookAxis = Value.Get<FVector2D>();

    AddYawInput(LookAxis.X * LookSensitivity);
    AddPitchInput(-LookAxis.Y * LookSensitivity);
}


void AMainGamePlayerController::OnMainGameRMBPressed(const FInputActionValue& Value)
{
    bRMBHeld = true;
    EnterCameraMode();
}


void AMainGamePlayerController::OnMainGameRMBReleased(const FInputActionValue& Value)
{
    bRMBHeld = false;
    EnterUIMode();
}


void AMainGamePlayerController::OnMainGameCheckCall(const FInputActionValue& Value)
{
    RequestCheckCall();
}


void AMainGamePlayerController::OnMainGameFold(const FInputActionValue& Value)
{
    RequestFold();
}


void AMainGamePlayerController::OnMainGameRaise(const FInputActionValue& Value)
{
    RequestRaise();
}


void AMainGamePlayerController::OnLobbyMove(const FInputActionValue& Value)
{
    const FVector2D MoveAxis = Value.Get<FVector2D>();
    if (APawn* MyPawn = GetPawn())
    {
        MyPawn->AddMovementInput(MyPawn->GetActorForwardVector(), MoveAxis.Y);
        MyPawn->AddMovementInput(MyPawn->GetActorRightVector(), MoveAxis.X);
    }
}


void AMainGamePlayerController::OnLobbyLook(const FInputActionValue& Value)
{
    const FVector2D LookAxis = Value.Get<FVector2D>();
    AddYawInput(LookAxis.X * LookSensitivity);
    AddPitchInput(-LookAxis.Y * LookSensitivity);
}