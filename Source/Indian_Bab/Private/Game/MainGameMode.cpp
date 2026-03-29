#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Character/LobbyCharacter.h"
#include "PlayerController/MainGamePlayerController.h"
#include "PlayerState/MainPlayerState.h"
#include "Character/LobbyCharacter.h"

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
		NextTurn();
		return;
	}
	if(Action == EBetAction::CheckCall)
	{
		NextTurn();
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

	NextTurn();
}

// 게임 시작 조건이 충족되었을 때 실행
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

		// 3초 뒤에 StartMainGame 함수 실행
		GetWorldTimerManager().ClearTimer(TimerHandle);
		GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::StartMainGame, 3.0f, false);
	}
}


// MainGame 시작
void AMainGameMode::StartMainGame()
{
	// TODO: 카드 분배, 앤티(Ante) 지불 등 실제 인게임 로직 호출, 생존자 카운팅
	AMainGameState* GS = GetGameState<AMainGameState>();
	CheckPlayer = -1;
	if (GS)
	{
		ResetFoldState();
		GS->SetGamePhase(EGamePhase::Playing);

		//처음에만 랜덤으로 플레이어 선택
		if (GS -> CurrentPlayerIndex == -1)
			PickRandomPlayer();
		
		// 타이머 시작
		if(GS -> CurrentTurnPlayerId != -1)
		{
			StartTurnTimer(20.0f);
		}
	}
}

// 랜덤한 플레이어 선택
void AMainGameMode::PickRandomPlayer()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	const int32 SeatedPlayerNum = GS-> SeatChairArray.Num();
	if (SeatedPlayerNum <= 0) return; // TODO: 테스트 추후 값 수정

	int32 CurrentPlayerIndex = FMath::RandRange(0, SeatedPlayerNum - 1);
	ASeatActor* CurrentChair = GS -> SeatChairArray[CurrentPlayerIndex];
	if(!CurrentChair || !CurrentChair -> GetOccupant()) return;

	ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(CurrentChair->GetOccupant());
	if (!OccupantCharacter) return;
	AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
	CheckPlayer = PS->GetPlayerId();
	GS -> ChangeGameTurn(PS -> GetPlayerId(), CurrentPlayerIndex);
}

// 턴 넘기는 타이머
void AMainGameMode::StartTurnTimer(float Time)
{
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::OnTurnTimerExpired, Time, false);
}

// 턴 제한시간은 넘겼을 때
void AMainGameMode::OnTurnTimerExpired()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	UE_LOG(LogTemp, Warning, TEXT("[GM] TimeOut NextTurn"));
	GS -> ChangeCurrentBetInfo(EBetAction::CheckCall);
	NextTurn();
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

// 다음 턴 갈 수 있는지 검사 -> NextTurn/NextRound/EndGame 판별 및 넘김
void AMainGameMode::NextTurn()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	const int32 NumSeats = GS->SeatChairArray.Num();
    if (NumSeats <= 0) return;
	
	// 생존 인원이 1명일 때
	if(GS -> AlivePlayerCount == 1)
	{
		// 게임 종료로 넘어감
		return;
	}
	GS->bTurnActionInProgress = false;

	// 한 명을 제외한 모든 인원이 폴드한 경우
	// 활성 인원이 1명 이하면 이번 라운드 종료
	int32 ActivePlayer = UpdateActivePlayer(GS);
	if(ActivePlayer <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Betting round ended: only one active player left"));
		NextRound();
		return;
	}

	// 다음 인원 선택
	int32 NextPlayerIndex = GS->CurrentPlayerIndex;
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

		// 종료 기준점 플레이어에게 다시 도달하면 한 라운드 종료
		if (NextPS->GetPlayerId() == CheckPlayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Betting round ended. Returned to checkpoint player: %d"), CheckPlayer);
			//TODO : 결과 확인
			// 현재는 NextRound만
			NextRound();
			return;
		}

		GS->ChangeGameTurn(NextPS->GetPlayerId(), NextPlayerIndex);
		StartTurnTimer(20.0f);
		return;
	}
}

// 결과확인
void AMainGameMode::CheckPlayerCard()
{

}

// 다음 라운드로 넘기는 함수
void AMainGameMode::NextRound()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	UE_LOG(LogTemp, Warning, TEXT("NextRound 함수 호출!"));
	
	//타이머 정리
	GetWorldTimerManager().ClearTimer(TimerHandle);

	// isFold 초기화
	ResetFoldState();

	// GateState초기화
	// 추후 GS로 옮길 예정
	GS->CurrentBetInfo.CurrentBetAction = EBetAction::None;
	GS->CurrentBetInfo.BetActionTotal = 0;
	GS->CurrentBulletCount = 1;

	// 플레이어 선택 코드
}

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
