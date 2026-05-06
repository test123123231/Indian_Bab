#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Character/LobbyCharacter.h"
#include "PlayerState/MainPlayerState.h"
#include "Character/LobbyCharacter.h"
#include "CardController/CardManager.h"

AMainGameMode::AMainGameMode()
{
	GameStateClass = AMainGameState::StaticClass();
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 접속한 플레이어 수 로그 (NumPlayers는 AGameMode 기본 변수)
	UE_LOG(LogTemp, Warning, TEXT("플레이어 접속 완료. 현재 인원: %d"), NumPlayers);
}

void AMainGameMode::PlayerSeated(APlayerController* SeatedPlayer, ASeatActor* SeatedChair)
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS && SeatedPlayer && SeatedChair)
	{
		// 중복 방지
		if(GS -> SeatChairArray.Contains(SeatedChair)) return;

		GS -> SeatChairArray.Add(SeatedChair);
	
		// SeatOrder 기준 정렬
        GS->SeatChairArray.Sort([](const ASeatActor& A, const ASeatActor& B)
        {
            return A.SeatOrder < B.SeatOrder;
        });

		int32 NewPlayerCount = GS -> SeatChairArray.Num();
		GS -> ChangeReadyPlayerCount(NewPlayerCount);

		CheckGameStart();
	}
}

// 게임 시작 조건이 충족되었을 때 실행 및 초기화
void AMainGameMode::CheckGameStart()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 기획 상 3~4인 플레이. 테스트를 위해 1인 이상으로 할 수도 있음.
	// 여기서는 현재 접속한 인원이 모두 앉았는지(Ready) 검사
	if (GS->ReadyPlayerCount >= 3 && GS->ReadyPlayerCount == NumPlayers) // TODO: 실제 서비스 시 >= 3으로 변경
	{
		UE_LOG(LogTemp, Warning, TEXT("모든 플레이어가 착석했습니다. 3초 후 게임을 시작합니다."));

		GS->SetGamePhase(EGamePhase::Starting);

		// 각 플레이어 서브 리볼버 초기화(제일 처음에만/ 매 라운드x)
		for(APlayerState* PS : GS -> PlayerArray)
		{
			AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
			if(MPS)
			{
				MPS->SetInitSubRevolver();
			}
		}

		// 기준 플레이어 초기화
		CheckPlayer = -1;

		//  카드 매니저 초기화
		MainCardManager = GetCardManager();
		if(!MainCardManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("No CardManager"));
			return;
		}
		MainCardManager -> InitializeDeck();

		// 3초 뒤에 StartMainGame 함수 실행
		GetWorldTimerManager().ClearTimer(TimerHandle);
		GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::StartMainGame, 3.0f, false);
	}
}

// MainGame 시작(반복됨)
void AMainGameMode::StartMainGame()
{
	// TODO: 카드 분배, 앤티(Ante) 지불 등 실제 인게임 로직 호출, 생존자 카운팅
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;
	
	GS->SetGamePhase(EGamePhase::Playing);

	//GS의 게임 페이즈 기반 플레이어 선택
	PickPlayer(GS -> CurrentPlayerIndex);
	
	// 라운드 내 isFold 초기화
	ResetFoldState();
	
	DistributeCard();
	
	StartTurnTimer(20.0f);

	return;
}

// 턴 넘기는 타이머
void AMainGameMode::StartTurnTimer(float Time)
{
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::OnTurnTimerExpired, Time, false);
	return;
}

// 턴 제한시간은 넘겼을 때
void AMainGameMode::OnTurnTimerExpired()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	UE_LOG(LogTemp, Warning, TEXT("[GM] TimeOut NextTurn"));
	GS -> ChangeCurrentBetInfo(EBetAction::CheckCall);
	CheckNext();
	return;
}

// 다음 행동(NextTurn, NextRound, ReStart) 체크 함수
void AMainGameMode::CheckNext()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	// 생존 인원이 1명일 때
	if(GS -> AlivePlayerCount == 1)
	{
		// 게임 종료로 넘어감
		return;
	}
	GS->bTurnActionInProgress = false;

	// 활성 인원(비폴드 + 생존)이 1명 이하면 이번 라운드 종료
	int32 ActivePlayer = UpdateActivePlayer(GS);
	if(ActivePlayer <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Betting round ended: only one active player left"));
		NextRound();
		return;
	}

	// 다음 플레이어 PS 획득
	AMainPlayerState* NextPS = GetNextPlayerState(GS->CurrentPlayerIndex);
	if(!NextPS) return;
	if (bCheckPlayerFolded)
	{
		CheckPlayer = NextPS->GetPlayerId();
		bCheckPlayerFolded = false;
		NextTurn(NextPS);
		return;
	}

	// CheckPlayer와 다음 플레이어가 동일 할 때 
	if(CheckPlayer == NextPS -> GetPlayerId())
	{
		CheckPlayerCard();
		return;
	}
	//CheckPlayer와 다음 플레이어가 다를 때
	else
	{
		NextTurn(NextPS);
		return;
	}
}

//격발 페이즈 관리
void AMainGameMode::ManageShotPhase()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    if (GS->CurrentGamePhase != EGamePhase::Result || !CurrentWinnerPS)
    {
		FinishMainShotPhase();
        return;
    }

    if (GS -> CurrentBulletCount <= 0)
    {
        FinishMainShotPhase();
        return;
    }
	else
	{
		StartMainshotTimer(10.0f);
	}
}

// 격발 페이즈 종료 후 정리
void AMainGameMode::FinishMainShotPhase()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(TimerHandle);

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    CurrentWinnerPS = nullptr;
	GS -> CurrentBulletCount = 0;

    NextRound();
}

// 다음 턴으로
void AMainGameMode::NextTurn(AMainPlayerState* NextPS)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	GS->ChangeGameTurn(NextPS->GetPlayerId(), GS -> CurrentPlayerIndex);
	StartTurnTimer(20.0f);
	return;
}

// 다음 라운드로 넘기는 함수
void AMainGameMode::NextRound()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;
	
	//타이머 정리
	GetWorldTimerManager().ClearTimer(TimerHandle);

	// 다음 라운드 대비 GateState 초기화
	GS -> SetNextRoundGameState();

	// 메인 게임 새로 시작
	StartMainGame();
}

// 라운드 내 isFold 초기화
void AMainGameMode::ResetFoldState()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	for (APlayerState* PS : GS->PlayerArray)
	{
		AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
		if (!MPS) continue;

		MPS->isFold = false;
	}
}
