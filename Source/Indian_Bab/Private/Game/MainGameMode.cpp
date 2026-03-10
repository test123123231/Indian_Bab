#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Character/LobbyCharacter.h"
#include "PlayerController/MainGamePlayerController.h"


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

    int32 PlayerId = RequestPC->GetPlayerIdSafe();

    const TCHAR* ActionStr = TEXT("UnKnown");
    if (Action == EBetAction::Raise)
        ActionStr = TEXT("Raise");
    else if (Action == EBetAction::CheckCall)
        ActionStr = TEXT("CheckCall");
    else if (Action == EBetAction::Fold)
        ActionStr = TEXT("Fold");

	if (GS -> CurrentTurnPlayerId == PlayerId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM][Input] PC = %d, Action = %s"),PlayerId, ActionStr);
		GS -> ShowCurrentBulletCount(Action);
		NextTurn();
	}
}

void AMainGameMode::CheckGameStart()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 기획 상 3~4인 플레이. 테스트를 위해 1인 이상으로 할 수도 있음.
	// 여기서는 현재 접속한 인원이 모두 앉았는지(Ready) 검사
	if (GS->ReadyPlayerCount >= 3 && GS->ReadyPlayerCount == NumPlayers)
	{
		UE_LOG(LogTemp, Warning, TEXT("모든 플레이어가 착석했습니다. 3초 후 게임을 시작합니다."));

		GS->SetGamePhase(EGamePhase::Starting);

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
			StartTurnTimer(5.0f);
	}
}

void AMainGameMode::PickRandomPlayer()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	const int32 SeatedPlayerNum = GS-> SeatChairArray.Num();
	if (SeatedPlayerNum <= 1) return;

	int32 CurrentPlayerIndex = FMath::RandRange(0, SeatedPlayerNum - 1);
	ASeatActor* CurrentChair = GS -> SeatChairArray[CurrentPlayerIndex];
	if(!CurrentChair || !CurrentChair -> GetOccupant()) return;

	ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(CurrentChair->GetOccupant());
	if (!OccupantCharacter) return;
	APlayerState* PS = OccupantCharacter -> GetPlayerState();
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

	UE_LOG(LogTemp, Warning, TEXT("[GM] TimeOut NextTurn"));
	NextTurn();
}

void AMainGameMode::NextTurn()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	const int32 NumSeats = GS->SeatChairArray.Num();
    if (NumSeats <= 0) return;

	//의자에 따라 순환
	int32 NextPlayerIndex = (GS -> CurrentPlayerIndex + 1) % NumSeats;
	ASeatActor* NextChair = GS -> SeatChairArray[NextPlayerIndex];
	if(!NextChair || !NextChair -> GetOccupant()) return;

	ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(NextChair->GetOccupant());
	if (!OccupantCharacter) return;
	APlayerState* NextPS = OccupantCharacter -> GetPlayerState();

	GS->ChangeGameTurn(NextPS->GetPlayerId(), NextPlayerIndex);
	StartTurnTimer(5.0f);
}
