#include "GameInstanceSubsystem/IndianBabGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogIndianBabGI, Log, All);

namespace
{
    FString NetworkFailureToString(ENetworkFailure::Type Type)
    {
        switch (Type)
        {
        case ENetworkFailure::NetDriverAlreadyExists:    return TEXT("NetDriverAlreadyExists");
        case ENetworkFailure::NetDriverCreateFailure:    return TEXT("NetDriverCreateFailure");
        case ENetworkFailure::NetDriverListenFailure:    return TEXT("NetDriverListenFailure");
        case ENetworkFailure::ConnectionLost:            return TEXT("ConnectionLost");
        case ENetworkFailure::ConnectionTimeout:         return TEXT("ConnectionTimeout");
        case ENetworkFailure::FailureReceived:           return TEXT("FailureReceived");
        case ENetworkFailure::OutdatedClient:            return TEXT("OutdatedClient");
        case ENetworkFailure::OutdatedServer:            return TEXT("OutdatedServer");
        case ENetworkFailure::PendingConnectionFailure:  return TEXT("PendingConnectionFailure");
        case ENetworkFailure::NetGuidMismatch:           return TEXT("NetGuidMismatch");
        case ENetworkFailure::NetChecksumMismatch:       return TEXT("NetChecksumMismatch");
        default:                                          return TEXT("Unknown");
        }
    }

    FString TravelFailureToString(ETravelFailure::Type Type)
    {
        switch (Type)
        {
        case ETravelFailure::NoLevel:                    return TEXT("NoLevel");
        case ETravelFailure::LoadMapFailure:             return TEXT("LoadMapFailure");
        case ETravelFailure::InvalidURL:                 return TEXT("InvalidURL");
        case ETravelFailure::PackageMissing:             return TEXT("PackageMissing");
        case ETravelFailure::PackageVersion:             return TEXT("PackageVersion");
        case ETravelFailure::NoDownload:                 return TEXT("NoDownload");
        case ETravelFailure::TravelFailure:              return TEXT("TravelFailure");
        case ETravelFailure::CheatCommands:              return TEXT("CheatCommands");
        case ETravelFailure::PendingNetGameCreateFailure:return TEXT("PendingNetGameCreateFailure");
        case ETravelFailure::CloudSaveFailure:           return TEXT("CloudSaveFailure");
        case ETravelFailure::ServerTravelFailure:        return TEXT("ServerTravelFailure");
        case ETravelFailure::ClientTravelFailure:        return TEXT("ClientTravelFailure");
        default:                                          return TEXT("Unknown");
        }
    }
}

void UIndianBabGameInstance::Init()
{
    Super::Init();

    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
            this, &UIndianBabGameInstance::OnNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
            this, &UIndianBabGameInstance::OnTravelFailure);

        UE_LOG(LogIndianBabGI, Log,
            TEXT("[GI] NetworkFailure/TravelFailure 핸들러 바인딩 완료."));
    }
}

void UIndianBabGameInstance::Shutdown()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }

    Super::Shutdown();
}

void UIndianBabGameInstance::OnNetworkFailure(UWorld* /*World*/, UNetDriver* /*NetDriver*/,
    ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    const FString TypeStr = NetworkFailureToString(FailureType);
    UE_LOG(LogIndianBabGI, Error,
        TEXT("[GI] NetworkFailure: type=%s err=%s"), *TypeStr, *ErrorString);

    // 데디 PreLogin 거부 분기:
    //   - PendingConnectionFailure: ClientTravel 후 PendingNetGame 단계에서 NMT_Failure 수신 (만석/AC 미검증/게임 진행 중)
    //   - FailureReceived: 이미 접속한 상태에서 서버가 NMT_Failure 발사 (예외 케이스)
    // 두 경우 모두 ErrorString이 데디 PreLogin의 ErrorMessage 그대로
    // → 인터넷 단절(NLA/ConnectionLost/Timeout)과 구분해 모달 채널로 분리.
    const bool bIsRejection =
        (FailureType == ENetworkFailure::PendingConnectionFailure) ||
        (FailureType == ENetworkFailure::FailureReceived);

    if (bIsRejection && !ErrorString.IsEmpty())
    {
        UE_LOG(LogIndianBabGI, Warning,
            TEXT("[GI] Dedi rejection — reason=%s"), *ErrorString);

        // 호스트/클라 모두 Steam Lobby 정리 + OnSessionErrorEvent broadcast가 CleanupHostSession에서 일괄 처리됨.
        // (호스트: 매치메이커 인스턴스/Lobby 폐기, 클라: 호스트 Lobby에서 leave → 슬롯 즉시 반환)
        if (USessionSubsystem* Session = GetSubsystem<USessionSubsystem>())
        {
            Session->CleanupHostSession(ErrorString);
        }
        return;
    }

    // 진짜 단절(NLA 사각지대 백스톱 포함) — 기존 경로
    if (UConnectivitySubsystem* Conn = GetSubsystem<UConnectivitySubsystem>())
    {
        Conn->ForceTriggerLost(FString::Printf(TEXT("NetworkFailure:%s"), *TypeStr));
    }
}

void UIndianBabGameInstance::OnTravelFailure(UWorld* /*World*/,
    ETravelFailure::Type FailureType, const FString& ErrorString)
{
    const FString TypeStr = TravelFailureToString(FailureType);
    UE_LOG(LogIndianBabGI, Error,
        TEXT("[GI] TravelFailure: type=%s err=%s"), *TypeStr, *ErrorString);

    if (UConnectivitySubsystem* Conn = GetSubsystem<UConnectivitySubsystem>())
    {
        Conn->ForceTriggerLost(FString::Printf(TEXT("TravelFailure:%s"), *TypeStr));
    }
}
