#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ConnectivitySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnConnectivityLost);
DECLARE_MULTICAST_DELEGATE(FOnConnectivityRestored);

/**
 * 인터넷 연결 상태를 런타임에 감시하는 서브시스템.
 *
 * Windows: INetworkListManager::get_IsConnectedToInternet() 를 1초마다 폴링.
 * 끊김 감지 후 2초 grace → OnConnectivityLost 발사 + 10초 후 RequestExit (총 12초).
 * 어느 단계든 회복되면 타이머 초기화 + OnConnectivityRestored 발사.
 *
 * IsOnline() 은 동기 조회 — 부팅 가드(UAntiCheatSubsystem::Initialize) 에서 사용.
 */
UCLASS()
class INDIAN_BAB_API UConnectivitySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 현재 인터넷 연결 여부 동기 조회 */
    bool IsOnline() const;

    /** 끊김 후 grace(2s) 경과 시 발사 */
    FOnConnectivityLost OnConnectivityLost;

    /** 연결 회복 시 발사 (grace 내 회복이면 발사하지 않음) */
    FOnConnectivityRestored OnConnectivityRestored;

private:
    /** 1초마다 FTicker 에서 호출 */
    bool OnPollTick(float DeltaTime);

    /** INetworkListManager COM 인터페이스로 인터넷 연결 여부 확인 */
    bool QueryInternetConnection() const;

    FTSTicker::FDelegateHandle PollHandle;

    enum class EConnState : uint8 { Online, Grace, Lost };
    EConnState State = EConnState::Online;

    double DisconnectTimestamp = 0.0; // grace 시작 시각 (FPlatformTime::Seconds)
    double LostTimestamp       = 0.0; // Lost 진입 시각

    static constexpr float GraceSeconds         = 2.0f;
    static constexpr float ExitAfterLostSeconds = 10.0f;
};
