#include "PlayerController/MainGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Game/MainGameMode.h"
#include "Game/MainGameTypes.h"
#include "GameFramework/PlayerState.h"
#include "Character/LobbyCameraManager.h"
#include "Character/LobbyCharacter.h"
#include "Interface/InteractableInterface.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "PlayerState/MainPlayerState.h"


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
    CreateDeckLeftWidget();
    EnterCameraMode();
	ApplyLobbyMappingContext();

    if (USettingSubsystem* SettingSS = GetGameInstance()->GetSubsystem<USettingSubsystem>())
    {
        LookSensitivity = SettingSS->GetMouseSensitivity();
    }

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;

    TrySendSteamNickname();
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
            EnhancedInput->BindAction(IA_MainGameCheckCall, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameCheckCall);
        }
        if (IA_MainGameFold)
        {
            EnhancedInput->BindAction(IA_MainGameFold, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameFold);
        }
        if (IA_MainGameRaise)
        {
            EnhancedInput->BindAction(IA_MainGameRaise, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameRaise);
        }

        if (IA_LobbyMove)
        {
            EnhancedInput->BindAction(IA_LobbyMove, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyMove);
        }

        if (IA_LobbyLook)
        {
            EnhancedInput->BindAction(IA_LobbyLook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyLook);
        }

        if (IA_MainGameTab)
        {
            EnhancedInput->BindAction(IA_MainGameTab, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameTabPressed);
        }

        if (IA_Fire)
        {
            EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AMainGamePlayerController::OnFire);
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

    if (MainGameWidgetInstance)
    {
        MainGameWidgetInstance->InitWidget();
    }
}

void AMainGamePlayerController::CreateDeckLeftWidget() 
{
    if (!DeckLeftWidgetClass) return;

    if (!DeckLeftWidgetInstance) 
    {
        DeckLeftWidgetInstance = CreateWidget<UDeckLeftWidget>(this, DeckLeftWidgetClass);
    }
    if (DeckLeftWidgetInstance && !DeckLeftWidgetInstance->IsInViewport()) 
    {
        DeckLeftWidgetInstance->AddToViewport();
    }
    if (DeckLeftWidgetInstance) 
    {
        DeckLeftWidgetInstance->InvisibleWidget();
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


void AMainGamePlayerController::RequestRaise(int32 RaiseCount)
{
    Server_RequestBetAction(EBetAction::Raise, RaiseCount);
}


void AMainGamePlayerController::RequestCheckCall()
{
    Server_RequestBetAction(EBetAction::CheckCall, 0);
}


void AMainGamePlayerController::RequestFold()
{
    Server_RequestBetAction(EBetAction::Fold, 0);
}

// 내 스팀 닉네임 읽기
FString AMainGamePlayerController::GetMySteamNickname() const
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem) return TEXT("");

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (Identity.IsValid())
    {
        return Identity->GetPlayerNickname(0);
    }

    return TEXT("");
}

void AMainGamePlayerController::TrySendSteamNickname()
{
    if (bSteamNicknameSent) return;

    if (!IsLocalPlayerController()) return;

    AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    const FString MyNickname = GetMySteamNickname();
    if (MyNickname.IsEmpty()) return;

    Server_SetSteamNickname(MyNickname);
    bSteamNicknameSent = true;
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
    RequestRaise(MainGameWidgetInstance->GetBetNum());
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

void AMainGamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (MainGameWidgetInstance)
    {
        MainGameWidgetInstance->InitWidget();
    }

    TrySendSteamNickname();
}

void AMainGamePlayerController::Server_RequestBetAction_Implementation(EBetAction Action, int32 RaiseCount)
{
    AMainGameMode* GM = GetWorld() -> GetAuthGameMode<AMainGameMode>();
    if(!GM) return;

     GM->HandleBetAction(this, Action, RaiseCount);
}

void AMainGamePlayerController::Server_SetSteamNickname_Implementation(const FString& NewNickname)
{
    AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    PS->SetSteamNickname(NewNickname);
}

void AMainGamePlayerController::Server_RequestMainRevolverShot_Implementation()
{
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM) return;

	GM->HandleMainRevolverShotAction(this);
}

void AMainGamePlayerController::ClientOnSeated_Implementation()
{
    // 로비 조작(WASD)을 끄고 메인 게임(마우스/UI) 조작으로 스위칭
    ApplyMainGameMappingContext();
    EnterUIMode();
}

int AMainGamePlayerController::GetPlayerIdSafe()
{
    const APlayerState* PS = GetPlayerState<APlayerState>();
    return PS ? PS->GetPlayerId() : -1;
}

void AMainGamePlayerController::OnMainGameTabPressed(const FInputActionValue& Value)
{

    if (DeckLeftWidgetInstance)
    {
        DeckLeftWidgetInstance->VisibleWidget();
    }
}

void AMainGamePlayerController::OnFire(const FInputActionValue& Value)
{
	Server_RequestMainRevolverShot();
}