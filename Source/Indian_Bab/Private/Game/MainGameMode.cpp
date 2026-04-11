#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Character/LobbyCharacter.h"
#include "PlayerController/MainGamePlayerController.h"
#include "PlayerState/MainPlayerState.h"
#include "Character/LobbyCharacter.h"
#include "Kismet/GameplayStatics.h"
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

// 폴드 애니메이션 끝났을 때 호출
void AMainGameMode::HandleFoldMontageFinished(ALobbyCharacter* Character)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;

	CheckNext();
}

// 게임 시작 조건이 충족되었을 때 실행 및 초기화
void AMainGameMode::CheckGameStart()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 기획 상 3~4인 플레이. 테스트를 위해 1인 이상으로 할 수도 있음.
	// 여기서는 현재 접속한 인원이 모두 앉았는지(Ready) 검사
	if (GS->ReadyPlayerCount >= 1 && GS->ReadyPlayerCount == NumPlayers) // TODO: 실제 서비스 시 >= 3으로 변경
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

}

// 카드 매니저 획득
ACardManager* AMainGameMode::GetCardManager()
{
    if (!MainCardManager)
    {
        MainCardManager = Cast<ACardManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ACardManager::StaticClass())
        );
    }
    return MainCardManager;
}


// 플레이어 선택
void AMainGameMode::PickPlayer(int32 CurrentPlayerIndex)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;
	if(GS -> CurrentGamePhase != EGamePhase::Playing) return;
	
	// 이제 막 게임 시작한 케이스
	if(CurrentPlayerIndex == -1 )
	{
		PickRandomPlayer();
	}
	// Result인 경우 결과 확인 페이즈인 케이스
	if(CurrentPlayerIndex != -1)
	{
		// 결과 판별 아직 안 만들어서 임시 구현
		PickRandomPlayer(); 
	}
	return;
}

// 카드 분배
void AMainGameMode::DistributeCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	DealtCards = MainCardManager->DealCards(GS->AlivePlayerCount);
	if (DealtCards.Num() != GS->AlivePlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("카드 배분 실패: Alive=%d, Dealt=%d"),
			GS->AlivePlayerCount, DealtCards.Num());
		return;
	}

	int32 CardIndex = 0;
	for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;

		PS -> SetMyCard(DealtCards[CardIndex++]);
		UE_LOG(LogTemp, Warning, TEXT("PS[%d] : PS_Card(%d, %s)"), PS->GetPlayerId(), PS->GetMyCard().Value, *PS->GetMyCard().Suit);
	}
	return;
}

// 플레이어 랜덤 선택
void AMainGameMode::PickRandomPlayer()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	const int32 SeatedPlayerNum = GS-> SeatChairArray.Num();
	if (SeatedPlayerNum <= 0) return;

	int32 CurrentPlayerIndex = FMath::RandRange(0, SeatedPlayerNum - 1);
	ASeatActor* CurrentChair = GS -> SeatChairArray[CurrentPlayerIndex];
	if(!CurrentChair || !CurrentChair -> GetOccupant()) return;

	ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(CurrentChair->GetOccupant());
	if (!OccupantCharacter) return;
	AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
	if(!PS) return;
	
	CheckPlayer = PS->GetPlayerId();
	GS -> ChangeGameTurn(PS -> GetPlayerId(), CurrentPlayerIndex);
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

// 활성 인원 업데이트
int32 AMainGameMode::UpdateActivePlayer(AMainGameState* GS)
{
    if (!GS) return 0;

	int32 ActivePlayer = 0;
	for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;
		if(PS -> isFold) continue;

		ActivePlayer++;
	}
	return ActivePlayer;
}

// 게임 결과 확인
void AMainGameMode::CheckPlayerCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	GS->SetGamePhase(EGamePhase::Result);

	/* 결과 관련 코드 작성*/

	// 일단 임시로 했음
	NextRound(GS);
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
		NextRound(GS);
		return;
	}

	// 다음 플레이어 PS 획득
	AMainPlayerState* NextPS = GetNextPlayerState(GS->CurrentPlayerIndex);
	if(!NextPS) return;

	// CheckPlayer와 다음 플레이어가 동일 할 때 
	if(CheckPlayer == NextPS -> GetPlayerId())
	{
		// 추후 결과 확인 구현
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

// 다음 플레이어 Get
// MainPlayerState* 리턴 및 GS의 CurrentPlayerIndex 업데이트
AMainPlayerState* AMainGameMode::GetNextPlayerState(int32 CurrentPlayerIndex)
{
	if (!HasAuthority()) return nullptr;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return nullptr;

	const int32 NumSeats = GS->SeatChairArray.Num();
    if (NumSeats <= 0) return nullptr;

	int32 NextPlayerIndex = CurrentPlayerIndex;
	for(int i = 0; i < NumSeats; i++)
	{
		NextPlayerIndex = (NextPlayerIndex + 1) % NumSeats;
		
		ASeatActor* NextChair = GS -> SeatChairArray[NextPlayerIndex];
		if(!NextChair || !NextChair -> GetOccupant()) continue;

		ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(NextChair->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* NextPS = OccupantCharacter->GetPlayerState<AMainPlayerState>();
		if (!NextPS) continue;
		if (!NextPS->isAlive) continue;
		if (NextPS->isFold) continue;

		GS -> CurrentPlayerIndex = NextPlayerIndex;
		return NextPS;
	}
	return nullptr;
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
void AMainGameMode::NextRound(AMainGameState* GS)
{
	if (!HasAuthority()) return;
	
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
