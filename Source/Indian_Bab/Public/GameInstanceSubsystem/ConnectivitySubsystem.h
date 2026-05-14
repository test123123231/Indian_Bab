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
 * **폴링 라이프사이클**: 부팅 시 자동 시작하지 않는다. 메인메뉴 위젯이 NativeConstruct
 * 에서 StartPolling(), NativeDestruct 에서 StopPolling() 호출. 데디 인게임 구간엔
 * 폴링이 꺼져있고 NetDriver 의 OnNetworkFailure(→ ForceTriggerLost) 가 책임진다.
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

    /** 폴링 시작 (이미 돌고 있으면 no-op). State 를 Online 으로 리셋. */
    void StartPolling();

    /** 폴링 정지 (안 돌고 있으면 no-op). State 는 보존 — 인게임 진입 시 호출. */
    void StopPolling();

    /**
     * 외부 시그널(NetDriver disconnect, TravelFailure 등)을 Lost 상태로 합류시킨다.
     * 이미 Lost 상태면 no-op. 폴링이 꺼져있어도 내부적으로 ticker 를 띄워서
     * Lost 카운트다운(10초 후 RequestExit) 을 보장한다.
     */
    void ForceTriggerLost(const FString& Reason);

    /** 끊김 후 grace(2s) 경과 시 발사 */
    FOnConnectivityLost OnConnectivityLost;

    /** 연결 회복 시 발사 (grace 내 회복이면 발사하지 않음) */
    FOnConnectivityRestored OnConnectivityRestored;

private:
    /** 1초마다 FTicker 에서 호출 */
    bool OnPollTick(float DeltaTime);

    /** INetworkListManager COM 인터페이스로 인터넷 연결 여부 확인 */
    bool QueryInternetConnection() const;

    /** ticker 등록만 보장 (State 는 안 건드림) — ForceTriggerLost 내부에서도 사용 */
    void EnsurePollerRunning();

    FTSTicker::FDelegateHandle PollHandle;

    enum class EConnState : uint8 { Online, Grace, Lost };
    EConnState State = EConnState::Online;

    double DisconnectTimestamp = 0.0; // grace 시작 시각 (FPlatformTime::Seconds)
    double LostTimestamp       = 0.0; // Lost 진입 시각

    static constexpr float GraceSeconds         = 2.0f;
    static constexpr float ExitAfterLostSeconds = 10.0f;
};
