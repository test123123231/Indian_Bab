#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Character/LobbyCharacter.h"
#include "Actor/SeatActor.h"
#include "PlayerState/MainPlayerState.h"

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
		PickByResult();
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

void AMainGameMode::PickByResult()
{
    if (!HasAuthority()) return;

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    AMainPlayerState* WinPS = MaxCardPlayer();
    if (!WinPS) return;
    
    for (int32 i = 0; i < GS->SeatChairArray.Num(); i++)
    {
        ASeatActor* Seat = GS->SeatChairArray[i];
        if (!Seat || !Seat->GetOccupant()) continue;

        ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(Seat->GetOccupant());
        if (!OccupantCharacter) continue;

        AMainPlayerState* PS = OccupantCharacter->GetPlayerState<AMainPlayerState>();
        if (!PS) continue;

        if (PS == WinPS)
        {
            CheckPlayer = PS->GetPlayerId();
            GS->ChangeGameTurn(PS->GetPlayerId(), i);

            UE_LOG(LogTemp, Warning, TEXT("[GM] PickByResult -> Player %d selected as next starter"), PS->GetPlayerId());
            return;
        }
    }
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

