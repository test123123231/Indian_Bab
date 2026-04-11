#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LobbyCharacter.h"
#include "Actor/SeatActor.h"
#include "Kismet/GameplayStatics.h"
#include "CardController/CardManager.h"
#include "PlayerState/MainPlayerState.h"

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

	/* 결과 관련 코드 작성*/

	// 일단 임시로 했음
	NextRound(GS);
}
