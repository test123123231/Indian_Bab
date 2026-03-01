#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
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


void AMainGameMode::PlayerSeated(APlayerController* SeatedPlayer)
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS)
	{
		GS->ReadyPlayerCount++;
		UE_LOG(LogTemp, Display, TEXT("플레이어 착석! 준비 인원: %d / %d"), GS->ReadyPlayerCount, NumPlayers);

		CheckGameStart();
	}
}

void AMainGameMode::HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

    int PlayerId = RequestPC->GetPlayerIdSafe();

    const TCHAR* ActionStr = TEXT("UnKown");
    if (Action == EBetAction::Raise)
        ActionStr = TEXT("Raise");
    else if (Action == EBetAction::CheckCall)
        ActionStr = TEXT("CheckCall");
    else if (Action == EBetAction::Fold)
        ActionStr = TEXT("Fold");

    UE_LOG(LogTemp, Warning, TEXT("[GM][Input] PC = %d, Action = %s"),PlayerId, ActionStr);
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
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::StartMainGame, 3.0f, false);
	}
}


void AMainGameMode::StartMainGame()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS)
	{
		GS->SetGamePhase(EGamePhase::Playing);

		// TODO: 카드 분배, 첫 번째 턴 플레이어 선정, 앤티(Ante) 지불 등 실제 인게임 로직 호출
	}
}
