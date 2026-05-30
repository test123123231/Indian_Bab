#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetworkDelegates.h"
#include "IndianBabGameInstance.generated.h"

class UNetDriver;

/**
 * 프로젝트 커스텀 GameInstance.
 *
 * 주 책임: UE 엔진의 NetworkFailure / TravelFailure 콜백을 받아서
 *         UConnectivitySubsystem::ForceTriggerLost 로 합류시킨다.
 *         (인터넷 단절 외에 데디 다운/타임아웃 케이스도 동일 흐름 처리)
 *
 * DefaultEngine.ini 의 [/Script/EngineSettings.GameMapsSettings] 또는
 * [/Script/Engine.GameInstance] 에 GameInstanceClass 로 등록되어야 활성됨.
 */
UCLASS()
class INDIAN_BAB_API UIndianBabGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

private:
    void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver,
        ENetworkFailure::Type FailureType, const FString& ErrorString);

    void OnTravelFailure(UWorld* World,
        ETravelFailure::Type FailureType, const FString& ErrorString);

    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
};
