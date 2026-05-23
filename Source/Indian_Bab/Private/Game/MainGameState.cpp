#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"

AMainGameState::AMainGameState()
{
	CurrentGamePhase = EGamePhase::Lobby;
	ReadyPlayerCount = 0;
	AlivePlayerCount = 0;
	CurrentTurnPlayerId = -1;
	bTurnActionInProgress = false;
	CurrentPlayerIndex = -1;
	CurrentBulletCount = 1;
	CurrentBetInfo.CurrentBetAction = EBetAction::None;
	CurrentBetInfo.BetActionTotal = 0;
	TimerEndServerTime = 0.0f;
	TimerDuration = 0.0f;
	bTimerActive = false;
}


void AMainGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수들을 클라이언트에게 복제(Replicate)하도록 등록
	DOREPLIFETIME(AMainGameState, CurrentGamePhase);
	DOREPLIFETIME(AMainGameState, AlivePlayerCount);
	DOREPLIFETIME(AMainGameState, ReadyPlayerCount);
	DOREPLIFETIME(AMainGameState, CurrentTurnPlayerId);
	DOREPLIFETIME(AMainGameState, CurrentPlayerIndex);
	DOREPLIFETIME(AMainGameState, CurrentBulletCount);
	DOREPLIFETIME(AMainGameState, CurrentBetInfo);
	DOREPLIFETIME(AMainGameState, bTurnActionInProgress);
	DOREPLIFETIME(AMainGameState, TimerEndServerTime);
	DOREPLIFETIME(AMainGameState, TimerDuration);
	DOREPLIFETIME(AMainGameState, bTimerActive);
}


void AMainGameState::SetGamePhase(EGamePhase NewPhase)
{
	// 오직 서버에서만 상태를 변경할 수 있음
	if (HasAuthority())
	{
		CurrentGamePhase = NewPhase;
		if (CurrentGamePhase == EGamePhase::Starting)
		{
			CurrentBulletCount = 1;
			OnRep_CurrentBulletCount();
		}

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_GamePhase();
	}
}

void AMainGameState::ChangeGameTurn(int32 NewTurnPlayerId, int32 NewPlayerIndex)
{
	if (HasAuthority())
	{
		CurrentTurnPlayerId = NewTurnPlayerId;
		CurrentPlayerIndex = NewPlayerIndex;

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_CurrentTurnPlayerId();
	}
}

void AMainGameState::ChangeCurrentBetInfo(EBetAction NewAction, int32 RaiseCount)
{
	if(!HasAuthority()) return;

	if(NewAction == EBetAction::Raise)
	{
		if(CurrentBulletCount + RaiseCount > 8) return;
		CurrentBulletCount += RaiseCount;

		OnRep_CurrentBulletCount();
	}

	CurrentBetInfo.CurrentBetAction = NewAction;
	CurrentBetInfo.BetActionTotal++;

	OnRep_CurrentBetInfo();
}

void AMainGameState::SetTimerInfo(float Time)
{
	TimerDuration = Time;
	TimerEndServerTime = GetServerWorldTimeSeconds() + Time;
	bTimerActive = true;

	OnRep_TimerInfo();
}

void AMainGameState::ClearTimerInfo()
{
	TimerDuration = 0.0f;
	TimerEndServerTime = 0.0f;
	bTimerActive = false;

	OnRep_TimerInfo();
}

float AMainGameState::GetRemainingTime() const
{
	if (!bTimerActive)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, TimerEndServerTime - GetServerWorldTimeSeconds());
}

int32 AMainGameState::GetRemainingTimeCeil() const
{
	return FMath::CeilToInt(GetRemainingTime());
}

void AMainGameState::ChangeReadyPlayerCount(int32 NewReadyCount)
{
	if (HasAuthority())
	{
		ReadyPlayerCount = NewReadyCount;
		AlivePlayerCount = ReadyPlayerCount;

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_ReadyPlayerCount();
	}
}

// 다음 라운드 위한 초기화
void AMainGameState::SetNextRoundGameState()
{
	CurrentBetInfo.CurrentBetAction = EBetAction::None;
	CurrentBetInfo.BetActionTotal = 0;
	CurrentBulletCount = 1;
	SetGamePhase(EGamePhase::Playing);
}

void AMainGameState::OnRep_CurrentTurnPlayerId()
{
	// 현재 턴의 플레이어 아이디 표시
    UE_LOG(LogTemp, Warning, TEXT("[GS]CurrentTurnPlayerId = %d"), CurrentTurnPlayerId);
}

void AMainGameState::OnRep_GamePhase()
{
	// TODO: 페이즈 변경 시 연출 (예: Playing이 되면 로비 UI 숨기고 메인 게임 UI 띄우기, 조명 어둡게 하기 등)
	if (CurrentGamePhase == EGamePhase::Starting)
	{
		// 호스트 클라가 자기 Steam Lobby를 잠금 — 비호스트는 내부 가드로 no-op
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (USessionSubsystem* Session = GI->GetSubsystem<USessionSubsystem>())
				{
					Session->LockSessionForInGame();
				}
			}
		}
	}
	if (CurrentGamePhase == EGamePhase::Playing)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS]=== 게임이 시작되었습니다! 장전된 총알 : %d==="), CurrentBulletCount);
	}
	if (CurrentGamePhase == EGamePhase::Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS]=== 결과 확인 중!"));
	}
}

void AMainGameState::OnRep_CurrentBetInfo()
{
	const TCHAR* ActionStr = TEXT("UnKnown");
    if (CurrentBetInfo.CurrentBetAction == EBetAction::Raise)
        ActionStr = TEXT("Raise");
    else if (CurrentBetInfo.CurrentBetAction == EBetAction::CheckCall)
        ActionStr = TEXT("CheckCall");
    else if (CurrentBetInfo.CurrentBetAction == EBetAction::Fold)
        ActionStr = TEXT("Fold");
	
	//UE_LOG(LogTemp, Warning, TEXT("[GS]BetAction = %s ActionTotal = %d CurrentBulletCount = %d"), ActionStr,CurrentBetInfo.BetActionTotal, CurrentBulletCount);
}

void AMainGameState::OnRep_AlivePlayerCount()
{
	UE_LOG(LogTemp, Warning, TEXT("현재 생존 인원 : %d"), AlivePlayerCount);
	const bool bIsGameInProgress = CurrentGamePhase != EGamePhase::Lobby;
	if (bIsGameInProgress && AlivePlayerCount <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS] 게임 종료, 생존 인원 : %d"), AlivePlayerCount);
	}
}

void AMainGameState::OnRep_ReadyPlayerCount()
{
	UE_LOG(LogTemp, Warning, TEXT("플레이어 착석! 준비 인원: %d / %d"), ReadyPlayerCount, PlayerArray.Num());
}

void AMainGameState::OnRep_CurrentBulletCount()
{
	UE_LOG(LogTemp, Warning, TEXT("[GS]누적된 방아쇠 당김 횟수: %d"), CurrentBulletCount);
}

void AMainGameState::OnRep_TimerInfo()
{
	UE_LOG(LogTemp, Warning, TEXT("[GS] TimerInfo Replicated. Active=%d Remaining=%d"),
		bTimerActive,
		GetRemainingTimeCeil()
	);
}
