#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyCharacter.h"
#include "PlayerState/MainPlayerState.h"
#include "Actor/Revolver.h"
#include "Kismet/GameplayStatics.h"

void AMainGameMode::HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 중복 입력 방지
	if (GS->bTurnActionInProgress) return;

    int32 PlayerId = RequestPC->GetPlayerIdSafe();
	if (GS -> CurrentTurnPlayerId != PlayerId) return;

	// Raise 불가능하면 아예 막고 종료
    if (Action == EBetAction::Raise && GS->CurrentBulletCount >= 8)
    {
		UE_LOG(LogTemp, Warning, TEXT("[GM] Raise blocked: CurrentBulletCount is already %d"), GS->CurrentBulletCount);
        //TODO 추후에 텍스트로 Raise 불가라고 뜨게
        return;
    }

	GS->bTurnActionInProgress = true;
	GS -> ChangeCurrentBetInfo(Action);
	UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d Action: %s"), PlayerId, *UEnum::GetValueAsString(Action));

	if(Action == EBetAction::Fold)
	{
		HandleFoldAction(RequestPC);
		return;
	}
	if(Action == EBetAction::Raise)
	{
		CheckPlayer = PlayerId;
		CheckNext();
		return;
	}
	if(Action == EBetAction::CheckCall)
	{
		CheckNext();
		return;
	}

	// 예외 상황 복구
	GS->bTurnActionInProgress = false;

}

// 폴드 베팅 액션
void AMainGameMode::HandleFoldAction(AMainGamePlayerController* RequestPC)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainPlayerState* PS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;
	
	if (CheckPlayer == PS->GetPlayerId())
	{
		bCheckPlayerFolded = true;
	}


	PS->isFold = true;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(RequestPC->GetPawn());
	if (Character)
	{
		Character->SetActiveRevolver(Character->DeskRevolver);
		Character->Multicast_PlayGrabGunMontage(EGunHoldReason::Fold);
	}

	bool PlayerAlive = PS -> ChangeSubRevolver();
	if(PlayerAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d survived by sub revolver"), PS ->GetPlayerId());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d died by sub revolver"), PS ->GetPlayerId());
		AMainGameState* GS = GetGameState<AMainGameState>();
		if(GS)
		{
			--GS -> AlivePlayerCount;
		}
	}
}

// 메인 리볼버 격발 시간 타이머
void AMainGameMode::StartMainshotTimer(float Time)
{
	if (!HasAuthority()) return;

	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::OnMainShotTimerExpired, Time, false);

}

// 메인 리볼버 격발 시간 넘겼을 때
void AMainGameMode::OnMainShotTimerExpired()
{
	if (!HasAuthority()) return;
	ExecuteMainShot(true);
}

// 메인 리볼버 격발 액션
void AMainGameMode::HandleMainRevolverShotAction(AMainGamePlayerController* RequestPC)
{
	if (!HasAuthority()) return;
	if (!RequestPC) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	AMainPlayerState* RequestPS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!RequestPS) return;

	// result 페이즈가 아니면 리턴
	if (GS->CurrentGamePhase != EGamePhase::Result) return;

	// 승리 플레이어나 요청한 플레이어가 승리 플레이어와 동일하지 않으면 리턴
	if (!CurrentWinnerPS || RequestPS != CurrentWinnerPS) return;

	if (GS -> CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}

	ExecuteMainShot(false);
}

// 폴드 애니메이션 끝났을 때 호출
void AMainGameMode::HandleFoldMontageFinished(ALobbyCharacter* Character)
{
    if (!HasAuthority()) return;
    if (!Character) return;

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;
	
    Character->Multicast_PutBackGunMontage(EGunHoldReason::Fold);
}

// 메인 리볼버 줍는 애니메이션 끝났을 때 호출
void AMainGameMode::HandleMainMontageFinished(ALobbyCharacter* Character)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;

	ManageShotPhase();
}

// 리볼버 격발 끝난 후(폴드나 게임 후)
void AMainGameMode::HandlePutBackGunMontageFinished(ALobbyCharacter* Character, EGunHoldReason Reason)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	if (Reason == EGunHoldReason::Fold)
	{
		CheckNext();
		return;
	}

	if (Reason == EGunHoldReason::Win)
	{
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}
}
void AMainGameMode::ExecuteMainShot(bool bAutoFire)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	if (!CurrentWinnerPS)
	{
		FinishMainShotPhase();
		return;
	}

	if (GS -> CurrentBulletCount <= 0)
	{
		FinishMainShotPhase();
		return;
	}
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GS -> CurrentBulletCount -= 1;

	//  TODO 라인 트레이스 및 사망처리
	
	if (GS->CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}

	ManageShotPhase();
}

ARevolver* AMainGameMode::GetMainRevolver()
{
	if (MainRevolver)
	{
		return MainRevolver;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(
		GetWorld(),
		FName("MainRevolver"),
		FoundActors
	);

	if (FoundActors.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] MainRevolver not found in map"));
		return nullptr;
	}

	MainRevolver = Cast<ARevolver>(FoundActors[0]);

	if (!MainRevolver)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] Found actor has MainRevolver tag but is not ARevolver"));
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver found: %s"), *GetNameSafe(MainRevolver));

	return MainRevolver;
}

void AMainGameMode::StartMainRevolverPutBack()
{
	if (!HasAuthority()) return;

	if (bMainRevolverPutBackInProgress) return;

	bMainRevolverPutBackInProgress = true;

	GetWorldTimerManager().ClearTimer(TimerHandle);

	if (!CurrentWinnerPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: CurrentWinnerPS is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: Winner PC is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	ALobbyCharacter* WinnerCharacter = Cast<ALobbyCharacter>(PC->GetPawn());
	if (!WinnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: WinnerCharacter is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] Start Main Revolver PutBack"));

	WinnerCharacter->Multicast_PutBackGunMontage(EGunHoldReason::Win);
}