#include "GameInstanceSubsystem/ConnectivitySubsystem.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <netlistmgr.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogConnectivity, Log, All);

void UConnectivitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 단계 A 부팅 가드: 오프라인이면 즉시 종료 (런타임 grace 없음).
    // Editor/PIE 는 제외 — 오프라인에서도 개발/테스트가 가능해야 한다.
#if WITH_EDITOR
    if (!QueryInternetConnection())
    {
        UE_LOG(LogConnectivity, Error, TEXT("[Connectivity] 부팅 시 인터넷 미연결. 게임 종료."));
        FPlatformMisc::RequestExit(false);
        return;
    }
    UE_LOG(LogConnectivity, Log, TEXT("[Connectivity] 부팅 시 인터넷 연결 확인."));
#endif

    State = EConnState::Online;
}

void UConnectivitySubsystem::Deinitialize()
{
    StopPolling();
    Super::Deinitialize();
}

void UConnectivitySubsystem::StartPolling()
{
    State = EConnState::Online;
    DisconnectTimestamp = 0.0;
    LostTimestamp       = 0.0;
    EnsurePollerRunning();
    UE_LOG(LogConnectivity, Log, TEXT("[Connectivity] 폴링 시작 (메인메뉴 활성)."));
}

void UConnectivitySubsystem::StopPolling()
{
    if (PollHandle.IsValid())
    {
        FTSTicker::RemoveTicker(PollHandle);
        PollHandle.Reset();
        UE_LOG(LogConnectivity, Log, TEXT("[Connectivity] 폴링 정지 (인게임/종료)."));
    }
}

void UConnectivitySubsystem::EnsurePollerRunning()
{
    if (!PollHandle.IsValid())
    {
        // 1초마다 폴링 — FTicker는 World 없이 GameInstance 레벨에서 동작
        PollHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UConnectivitySubsystem::OnPollTick),
            1.0f);
    }
}

bool UConnectivitySubsystem::IsOnline() const
{
    return QueryInternetConnection();
}

void UConnectivitySubsystem::ForceTriggerLost(const FString& Reason)
{
    // 이미 Lost면 중복 트리거 방지 (폴링이 이미 카운트다운 중)
    if (State == EConnState::Lost)
    {
        UE_LOG(LogConnectivity, Verbose,
            TEXT("[Connectivity] ForceTriggerLost 무시(이미 Lost): %s"), *Reason);
        return;
    }

    UE_LOG(LogConnectivity, Error,
        TEXT("[Connectivity] ForceTriggerLost: %s — Lost 상태 진입, %.0fs 후 종료."),
        *Reason, ExitAfterLostSeconds);

    State         = EConnState::Lost;
    LostTimestamp = FPlatformTime::Seconds();
    OnConnectivityLost.Broadcast();

    // 폴링이 꺼진 인게임 구간에서도 Lost 카운트다운(10초 후 RequestExit)이 돌아야 함
    EnsurePollerRunning();
}

bool UConnectivitySubsystem::QueryInternetConnection() const
{
#if PLATFORM_WINDOWS
    INetworkListManager* NLM = nullptr;
    const HRESULT HR = CoCreateInstance(
        __uuidof(NetworkListManager), nullptr,
        CLSCTX_ALL,
        __uuidof(INetworkListManager),
        reinterpret_cast<void**>(&NLM));

    if (FAILED(HR) || !NLM)
    {
        // COM 실패 = 상태 불명 → 보수적으로 오프라인 처리
        UE_LOG(LogConnectivity, Warning, TEXT("[Connectivity] CoCreateInstance 실패 (hr=0x%08X), 오프라인으로 가정."), HR);
        return false;
    }

    VARIANT_BOOL bConnected = VARIANT_FALSE;
    NLM->get_IsConnectedToInternet(&bConnected);
    NLM->Release();

    return bConnected == VARIANT_TRUE;
#else
    return true;
#endif
}

bool UConnectivitySubsystem::OnPollTick(float /*DeltaTime*/)
{
    const bool bOnline = QueryInternetConnection();
    const double Now   = FPlatformTime::Seconds();

    switch (State)
    {
    case EConnState::Online:
        if (!bOnline)
        {
            UE_LOG(LogConnectivity, Warning,
                TEXT("[Connectivity] 인터넷 끊김 감지. %.0fs grace 시작."), GraceSeconds);
            State              = EConnState::Grace;
            DisconnectTimestamp = Now;
        }
        break;

    case EConnState::Grace:
        if (bOnline)
        {
            UE_LOG(LogConnectivity, Log, TEXT("[Connectivity] grace 내 회복."));
            State = EConnState::Online;
        }
        else if (Now - DisconnectTimestamp >= GraceSeconds)
        {
            UE_LOG(LogConnectivity, Error,
                TEXT("[Connectivity] grace 만료. OnConnectivityLost 발사. %.0fs 후 종료."),
                ExitAfterLostSeconds);
            State        = EConnState::Lost;
            LostTimestamp = Now;
            OnConnectivityLost.Broadcast();
        }
        break;

    case EConnState::Lost:
        if (bOnline)
        {
            UE_LOG(LogConnectivity, Log, TEXT("[Connectivity] 인터넷 복구. OnConnectivityRestored 발사."));
            State = EConnState::Online;
            OnConnectivityRestored.Broadcast();
        }
        else if (Now - LostTimestamp >= ExitAfterLostSeconds)
        {
            UE_LOG(LogConnectivity, Error, TEXT("[Connectivity] 10초 복구 없음. 게임 종료."));
            FPlatformMisc::RequestExit(false);
        }
        break;
    }

    return true; // ticker 유지
}
