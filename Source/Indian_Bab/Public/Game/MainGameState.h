#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
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
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 ReadyPlayerCount;

	// 서버가 GamePhase를 변경할 때 호출
	void SetGamePhase(EGamePhase NewPhase);

protected:
	// 클라이언트에서 GamePhase가 변경되었을 때 실행될 로직 (UI 업데이트 등)
	UFUNCTION()
	void OnRep_GamePhase();
};
