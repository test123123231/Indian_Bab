#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyCharacter.h"
#include "PlayerState/MainPlayerState.h"

void AMainGameMode::HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 중복 입력 방지
	if (GS->bTurnActionInProgress) return;

    int32 PlayerId = RequestPC->GetPlayerIdSafe();
	if (GS -> CurrentTurnPlayerId != PlayerId) return;

	// Raise 불가능하면 아예 막고 종료
    if (Action == EBetAction::Raise && GS->CurrentBulletCount >= 8)
    {
		UE_LOG(LogTemp, Warning, TEXT("[GM] Raise blocked: CurrentBulletCount is already %d"), GS->CurrentBulletCount);
        //TODO 추후에 텍스트로 Raise 불가라고 뜨게
        return;
    }

	GS->bTurnActionInProgress = true;
	GS -> ChangeCurrentBetInfo(Action);
	UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d Action: %s"), PlayerId, *UEnum::GetValueAsString(Action));

	if(Action == EBetAction::Fold)
	{
		HandleFoldAction(RequestPC);
		return;
	}
	if(Action == EBetAction::Raise)
	{
		CheckPlayer = PlayerId;
		CheckNext();
		return;
	}
	if(Action == EBetAction::CheckCall)
	{
		CheckNext();
		return;
	}

	// 예외 상황 복구
	GS->bTurnActionInProgress = false;

}

// 폴드 베팅 액션
void AMainGameMode::HandleFoldAction(AMainGamePlayerController* RequestPC)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainPlayerState* PS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	PS->isFold = true;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(RequestPC->GetPawn());
	if (Character)
	{
		Character->Multicast_PlayGrabGunMontage(EGunHoldReason::Fold);
	}

	bool PlayerAlive = PS -> ChangeSubRevolver();
	if(PlayerAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d survived by sub revolver"), PS ->GetPlayerId());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d died by sub revolver"), PS ->GetPlayerId());
		AMainGameState* GS = GetGameState<AMainGameState>();
		if(GS)
		{
			--GS -> AlivePlayerCount;
		}
	}
}

// 폴드 애니메이션 끝났을 때 호출
void AMainGameMode::HandleFoldMontageFinished(ALobbyCharacter* Character)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;

	CheckNext();
}