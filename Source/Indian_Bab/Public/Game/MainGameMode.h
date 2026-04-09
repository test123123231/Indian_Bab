#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Game/MainGameTypes.h"
#include "CardController/CardManager.h"
#include "MainGameMode.generated.h"

class AMainGamePlayerController;
class ASeatActor;
class ALobbyCharacter;
class AMainGameState;
class AMainPlayerState;

UCLASS()
class INDIAN_BAB_API AMainGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMainGameMode();

	// 플레이어가 서버에 접속 완료했을 때 호출됨
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 플레이어가 의자에 앉았을 때 (Controller나 Character에서) 호출됨
	void PlayerSeated(APlayerController* SeatedPlayer, ASeatActor* SeatedChair);

	// 베팅 액션 관리
	void HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action);

	// 폴드 베팅 액션
	void HandleFoldAction(AMainGamePlayerController* RequestPC);

	// 폴드 행동이 끝났을 때
	void HandleFoldMontageFinished(ALobbyCharacter* Character);

protected:
	// 전원 준비되었는지 체크하고 게임을 시작하는 함수
	void CheckGameStart();

	// 게임 루프 시작점(여기로 안 돌아옴)
	void StartMainGame();

	// 카드 매니저 획득
	ACardManager* GetCardManager();

	// 플레이어 선택
	void PickPlayer(int32 CurrentPlayerIndex);

	// 플레이어 랜덤 선택
	void PickRandomPlayer();

	// 턴 넘기는 타이머
	void StartTurnTimer(float Time);

	// 턴 제한시간은 넘겼을 때
	void OnTurnTimerExpired();

	// 활성 인원 업데이트
	int32 UpdateActivePlayer(AMainGameState* GS);

	// 결과확인
	void CheckPlayerCard();

	// 다음 행동(NextTurn, NextRound, ReStart) 체크 함수
	void CheckNext();

	// 다음 플레이어 PS Get
	AMainPlayerState* GetNextPlayerState(int32 CurrentPlayerIndex);
	
	// 다음 턴
	void NextTurn(AMainPlayerState* NextPS);

	// 다음 라운드
	void NextRound(AMainGameState* GS);

	// 폴드 인원 초기화
	void ResetFoldState();

private:
	FTimerHandle TimerHandle;

	// 베팅 기준점 플레이어
	int32 CheckPlayer;

	UPROPERTY()
	TObjectPtr<ACardManager> MainCardManager;

	UPROPERTY()
	TArray<FCardData> DealtCards;

};
