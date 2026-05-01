#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyCharacter.h"
#include "PlayerState/MainPlayerState.h"

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

	PS->isFold = true;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(RequestPC->GetPawn());
	if (Character)
	{
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
		FinishMainShotPhase();
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
	if(!GS) return;

	CheckNext();
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

	ManageShotPhase();
}