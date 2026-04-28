#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LobbyCharacter.h"
#include "Actor/SeatActor.h"
#include "Kismet/GameplayStatics.h"
#include "CardController/CardManager.h"
#include "PlayerState/MainPlayerState.h"

// 카드 매니저 획득
TObjectPtr<ACardManager> AMainGameMode::GetCardManager()
{
    if (!MainCardManager)
    {
        MainCardManager = Cast<ACardManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ACardManager::StaticClass())
        );
    }
    return MainCardManager;
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

// 게임 결과 확인
void AMainGameMode::CheckPlayerCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	GS->SetGamePhase(EGamePhase::Result);

    CurrentWinnerPS = MaxCardPlayer();
	if(!CurrentWinnerPS)
	{
		NextRound(GS);
        return;
	}
    UE_LOG(LogTemp, Warning, TEXT("Winner : %d[%d, %s]"), CurrentWinnerPS -> GetPlayerId(), CurrentWinnerPS->GetMyCard().Value, *CurrentWinnerPS->GetMyCard().Suit);

	StartMainShotPhase(CurrentWinnerPS);
}

// 활성 인원 중에서 가장 큰 값을 가진 플레이어
TObjectPtr<AMainPlayerState> AMainGameMode::MaxCardPlayer()
{
    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return nullptr;

    AMainPlayerState* MaxPS = nullptr;
    FCardData MaxCard;

    bool bFound = false;

    for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ALobbyCharacter* OccupantCharacter = Cast<ALobbyCharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;
        if(PS -> isFold) continue;

        FCardData CurrentCard = PS->GetMyCard();

        if (!bFound || MainCardManager -> IsCardHigher(CurrentCard, MaxCard))
        {
            MaxCard = CurrentCard;
            MaxPS = PS;
            bFound = true;
        }
	}

    return MaxPS;
}