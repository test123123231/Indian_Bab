#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "Net/UnrealNetwork.h"

AMainGameState::AMainGameState()
{
	CurrentGamePhase = EGamePhase::Lobby;
	ReadyPlayerCount = 0;
	CurrentTurnPlayerId = -1;
	CurrentPlayerIndex = -1;
	CurrentBulletCount = 1;
}


void AMainGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수들을 클라이언트에게 복제(Replicate)하도록 등록
	DOREPLIFETIME(AMainGameState, CurrentGamePhase);
	DOREPLIFETIME(AMainGameState, ReadyPlayerCount);
	DOREPLIFETIME(AMainGameState, CurrentTurnPlayerId);
	DOREPLIFETIME(AMainGameState, CurrentPlayerIndex);
	DOREPLIFETIME(AMainGameState, CurrentBulletCount);
}


void AMainGameState::SetGamePhase(EGamePhase NewPhase)
{
	// 오직 서버에서만 상태를 변경할 수 있음
	if (HasAuthority())
	{
		CurrentGamePhase = NewPhase;

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_GamePhase();
	}
}

void AMainGameState::ChangeGameTurn(int32 NewTurnPlayerId, int32 NewPlayerIndex)
{
	if (HasAuthority())
	{
		CurrentTurnPlayerId = NewTurnPlayerId;
		CurrentPlayerIndex = NewPlayerIndex;

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_CurrentTurnPlayerId();
	}
}

void AMainGameState::ShowCurrentBulletCount(EBetAction Action)
{
	if(!HasAuthority()) return;

	if(Action == EBetAction::Raise && CurrentBulletCount < 8)
	{
		CurrentBulletCount++;

		// 서버 자신도 UI나 연출 업데이트를 위해 OnRep 함수 수동 호출
		OnRep_CurrentBulletCount();
	}
}

void AMainGameState::OnRep_CurrentTurnPlayerId()
{
	// 현재 턴의 플레이어 아이디 표시
    UE_LOG(LogTemp, Warning, TEXT("[GS]CurrentTurnPlayerId = %d"), CurrentTurnPlayerId);
}

void AMainGameState::OnRep_GamePhase()
{
	// TODO: 페이즈 변경 시 연출 (예: Playing이 되면 로비 UI 숨기고 메인 게임 UI 띄우기, 조명 어둡게 하기 등)
	if (CurrentGamePhase == EGamePhase::Playing)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GS]=== 게임이 시작되었습니다! 장전된 총알 : %d==="), CurrentBulletCount);
	}
}

void AMainGameState::OnRep_CurrentBulletCount()
{
	UE_LOG(LogTemp, Warning, TEXT("[GS]BulletCount = %d"), CurrentBulletCount);
}