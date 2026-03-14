#include "Game/MainGameState.h"
#include "Net/UnrealNetwork.h"


AMainGameState::AMainGameState()
{
	CurrentGamePhase = EGamePhase::Lobby;
	ReadyPlayerCount = 0;
}


void AMainGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수들을 클라이언트에게 복제(Replicate)하도록 등록
	DOREPLIFETIME(AMainGameState, CurrentGamePhase);
	DOREPLIFETIME(AMainGameState, ReadyPlayerCount);
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


void AMainGameState::OnRep_GamePhase()
{
	// TODO: 페이즈 변경 시 연출 (예: Playing이 되면 로비 UI 숨기고 메인 게임 UI 띄우기, 조명 어둡게 하기 등)
	if (CurrentGamePhase == EGamePhase::Playing)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== 게임이 시작되었습니다! ==="));
	}
}