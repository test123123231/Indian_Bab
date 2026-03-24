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

	if (GS->bTurnActionInProgress) return;

    int32 PlayerId = RequestPC->GetPlayerIdSafe();
	if (GS -> CurrentTurnPlayerId != PlayerId) return;

	GS->bTurnActionInProgress = true;

	GS -> ChangeCurrentBetInfo(Action);
	UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d Action: %s"), PlayerId, *UEnum::GetValueAsString(Action));

	if(Action == EBetAction::Fold)
	{
		HandleFoldAction(RequestPC);
		return;
	}
	else
	{
		NextTurn();
		return;
	}
}

// 폴드 베팅 액션
void AMainGameMode::HandleFoldAction(AMainGamePlayerController* RequestPC)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainPlayerState* PS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;

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
		--GS -> AlivePlayerCount;
	}
}

void AMainGameMode::HandleFoldMontageFinished(ALobbyCharacter* Character)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;

	if (GS->AlivePlayerCount > 1)
	{
		NextTurn();
	}
	else
	{
		
	}
}

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

void AMainGameMode::StartMainGame()
{
	// TODO: 카드 분배, 앤티(Ante) 지불 등 실제 인게임 로직 호출, 생존자 카운팅
	AMainGameState* GS = GetGameState<AMainGameState>();

	if (GS)
	{
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

void AMainGameMode::NextTurn()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	const int32 NumSeats = GS->SeatChairArray.Num();
    if (NumSeats <= 0) return;
	if(GS -> AlivePlayerCount == 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d Winner!"), GS->CurrentTurnPlayerId);
		return;
	}

	int32 NextPlayerIndex = GS -> CurrentPlayerIndex;
	GS->bTurnActionInProgress = false;

	//의자에 따라 순환
	for(int i = 0; i < NumSeats; i++)
	{
		NextPlayerIndex = (NextPlayerIndex + 1) % NumSeats;
		
		ASeatActor* NextChair = GS -> SeatChairArray[NextPlayerIndex];
		if(!NextChair || !NextChair -> GetOccupant()) continue;

		ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(NextChair->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* NextPS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!NextPS) continue;
		if(!NextPS -> isAlive) continue;

		GS->ChangeGameTurn(NextPS->GetPlayerId(), NextPlayerIndex);
		StartTurnTimer(20.0f);
		return;
	}
}
