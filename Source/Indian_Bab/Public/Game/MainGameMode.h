#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Game/MainGameTypes.h"
#include "MainGameMode.generated.h"

class AMainGamePlayerController;

UCLASS()
class INDIAN_BAB_API AMainGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMainGameMode();

	// 플레이어가 서버에 접속 완료했을 때 호출됨
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 플레이어가 의자에 앉았을 때 (Controller나 Character에서) 호출됨
	void PlayerSeated(APlayerController* SeatedPlayer);

	// 베팅 액션 관리
	void HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action);

protected:
	// 전원 준비되었는지 체크하고 게임을 시작하는 함수
	void CheckGameStart();

	// 게임 루프 시작
	void StartMainGame();
};
