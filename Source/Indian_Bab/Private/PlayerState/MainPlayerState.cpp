#include "PlayerState/MainPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Game/MainGameState.h"

AMainPlayerState::AMainPlayerState()
{
    isAlive = 1;
    TotalTriggerCount = 0;
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수들을 클라이언트에게 복제(Replicate)하도록 등록
    DOREPLIFETIME(AMainPlayerState, isAlive);
    DOREPLIFETIME(AMainPlayerState, BulletArray);
    DOREPLIFETIME(AMainPlayerState, TotalTriggerCount);
}

// 처음 서브 리볼버 설정
void AMainPlayerState::SetInitSubRevolver()
{
    BulletArray.SetNum(8);

    for (int32 i = 0; i < BulletArray.Num(); i++)
    {
        BulletArray[i] = 0;
    }
    int32 RandomIndex = FMath::RandRange(0, BulletArray.Num() - 1);
    BulletArray[RandomIndex] = 1;
    TotalTriggerCount = 0;
    isAlive = 1;
}

bool AMainPlayerState::ChangeSubRevolver()
{
    if(BulletArray[TotalTriggerCount])
    {
        isAlive = 0;
        OnRep_isAlive();
    }

    TotalTriggerCount++;
    OnRep_TotalTriggerCount();

    return isAlive;
}

void AMainPlayerState::OnRep_TotalTriggerCount()
{
    UE_LOG(LogTemp, Warning, TEXT("[PS_%d] : %d / 8"), GetPlayerId(), TotalTriggerCount);
}

void AMainPlayerState::OnRep_isAlive()
{
    UE_LOG(LogTemp, Warning, TEXT("[PS_%d] : dead!"), GetPlayerId());
	AMainGameState* GS = GetGameState<AMainGameState>();
    GS -> AlivePlayerCount--;
}
