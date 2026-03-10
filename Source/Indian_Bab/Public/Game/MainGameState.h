#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "Actor/SeatActor.h"
#include "Game/MainGameTypes.h"
#include "MainGameState.generated.h"


// 게임의 현재 진행 단계를 정의하는 Enum
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Lobby UMETA(DisplayName = "로비 (대기중)"),
	Starting UMETA(DisplayName = "시작 카운트다운"),
	Playing UMETA(DisplayName = "게임 진행중"),
	Result UMETA(DisplayName = "결과창")
};


UCLASS()
class INDIAN_BAB_API AMainGameState : public AGameState
{
	GENERATED_BODY()

public:
	AMainGameState();

	// 상태 복제를 위한 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 게임 단계
	UPROPERTY(ReplicatedUsing = OnRep_GamePhase, BlueprintReadOnly, Category = "Game State")
	EGamePhase CurrentGamePhase;

	// 현재 의자에 앉아 준비를 마친 플레이어 수
	UPROPERTY(ReplicatedUsing = OnRep_ReadyPlayerCount, BlueprintReadOnly, Category = "Game State")
	int32 ReadyPlayerCount;

	// 현재 턴의 플레이어
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTurnPlayerId, BlueprintReadOnly, Category = "Game State")
	int32 CurrentTurnPlayerId;

	// 현재 턴의 플레이어의 인덱스(SeatChairArray 인덱스)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 CurrentPlayerIndex;

	// 의자 배열(서버 전용 replicated 현재로선 불필요)
	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	TArray<ASeatActor*> SeatChairArray;

	// 현재 리볼버에 장전된 탄환의 개수
	UPROPERTY(ReplicatedUsing = OnRep_CurrentBulletCount, BlueprintReadOnly, Category = "Game State")
	int32 CurrentBulletCount;


	// 서버가 GamePhase를 변경할 때 호출
	void SetGamePhase(EGamePhase NewPhase);

	// 서버에서 턴이 바뀔 때 호출
	void ChangeGameTurn(int32 NewTurnPlayerId, int32 NewPlayerIndex);

	// 서버에서 탄환 개수 호출
	void ShowCurrentBulletCount(EBetAction Action);

	// 서버에서 준비 인원 호출
	void ChangeReadyPlayerCount(int NewReadyCount);


protected:
	// 클라이언트에서 GamePhase가 변경되었을 때 실행될 로직 (UI 업데이트 등)
	UFUNCTION()
	void OnRep_GamePhase();

	// 클라이언트에서 플레이어 ID가 변경되었을 때 실행
	UFUNCTION()
	void OnRep_CurrentTurnPlayerId();

	// 클라이언트에서 탄환의 개수가 변경되었을 때 실행
	UFUNCTION()
	void OnRep_CurrentBulletCount();

	// 클라이언트에서 준비 인원(앉은 인원)이 변경되었을 때 실행
	UFUNCTION()
	void OnRep_ReadyPlayerCount();
};
