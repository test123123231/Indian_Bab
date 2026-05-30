#include "GameInstanceSubsystem/DediKickEndpointSubsystem.h"

#include "Network/NetworkEndpoints.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "Internationalization/Text.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/CoreOnline.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "IPAddress.h"

#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpRouteHandle.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpPath.h"
#include "HttpResultCallback.h"

#include "HAL/PlatformMisc.h"
#include "CoreGlobals.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDediKick, Log, All);

#if !WITH_SERVER_CODE
// 클라 타겟(WITH_SERVER_CODE=0): vtable 슬롯 채우기용 빈 stub.
// ShouldCreateSubsystem=false 라 Initialize/Deinitialize 는 실제 호출되지 않음.
bool UDediKickEndpointSubsystem::ShouldCreateSubsystem(UObject* /*Outer*/) const { return false; }
void UDediKickEndpointSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDediKickEndpointSubsystem::Deinitialize() { Super::Deinitialize(); }
#endif

#if WITH_SERVER_CODE

namespace
{
    constexpr uint16 kGamePortDefault = 7777;

    // AntiCheat / SteamCredentials 부팅 가드와 동일 패턴. 데디는 단일 로컬 머신
    // 운영(운영자 화면 가시) 가정이라 MessageBox UX 유지. bForce=true 로 즉시
    // TerminateProcess — 부팅 단계라 cleanup 손실 없음. MM 측은 데디 프로세스가
    // 모달로 인해 살아있는 동안에도 내부 HTTP 포트가 안 열리는 것으로 readiness
    // 프로브에서 실패를 감지(이 파일 헤더 코멘트 + match.py readiness 로직 참조).
    void DediFatalExitWithDialog(const TCHAR* Title, const TCHAR* Body, const TCHAR* LogReason)
    {
#if PLATFORM_WINDOWS
        ::MessageBoxW(nullptr, Body, Title, MB_OK | MB_ICONERROR);
#endif
        UE_LOG(LogDediKick, Error, TEXT("[DediKick] %s. 데디 종료."), LogReason);
        FPlatformMisc::RequestExit(true);
    }

    bool IsLoopback(const TSharedPtr<FInternetAddr>& Addr)
    {
        if (!Addr.IsValid()) return false;
        const FString Ip = Addr->ToString(false); // host only
        // IPv4 127.0.0.0/8 + IPv6 ::1. 충분한 약식 매칭 — Listen 자체가 loopback iface 만이라
        // 사실상 도달 가능한 PeerAddress 가 이 둘 뿐.
        return Ip.StartsWith(TEXT("127.")) || Ip == TEXT("::1");
    }

    TUniquePtr<FHttpServerResponse> JsonResponse(int32 Code, const FString& Body)
    {
        TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Body, TEXT("application/json"));
        Resp->Code = static_cast<EHttpServerResponseCodes>(Code);
        return Resp;
    }
}

uint16 UDediKickEndpointSubsystem::ResolveHttpPort()
{
    int32 GamePort = kGamePortDefault;
    FParse::Value(FCommandLine::Get(), TEXT("port="), GamePort);
    return static_cast<uint16>(GamePort + NetworkEndpoints::Dedi::Internal::kHttpPortOffset);
}

bool UDediKickEndpointSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // 데디 단독. 클라/에디터/PIE 는 인스턴스 0.
    return IsRunningDedicatedServer();
}

void UDediKickEndpointSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    BoundPort = ResolveHttpPort();

    FHttpServerModule& Module = FHttpServerModule::Get();
    Router = Module.GetHttpRouter(BoundPort, /*bFailOnBindFailure*/ true);
    if (!Router.IsValid())
    {
        const FString Detail = FString::Printf(
            TEXT("데디 내부 HTTP 포트(%u) 바인드에 실패했습니다.\n다른 프로세스가 점유 중인지 확인하세요.\n데디를 종료합니다."),
            BoundPort);
        DediFatalExitWithDialog(
            TEXT("Indian_Bab Dedi — 내부 엔드포인트 초기화 실패"),
            *Detail,
            *FString::Printf(TEXT("GetHttpRouter 실패 port=%u"), BoundPort));
        return;
    }

    RouteHandle = Router->BindRoute(
        FHttpPath(NetworkEndpoints::Dedi::Internal::KickPlayerRoutePattern()),
        EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateUObject(this, &UDediKickEndpointSubsystem::HandleKickPlayer));

    if (!RouteHandle.IsValid())
    {
        Router.Reset();
        DediFatalExitWithDialog(
            TEXT("Indian_Bab Dedi — 내부 엔드포인트 초기화 실패"),
            TEXT("Kick 라우트 바인드에 실패했습니다.\n데디를 종료합니다."),
            *FString::Printf(TEXT("BindRoute 실패 port=%u"), BoundPort));
        return;
    }

    Module.StartAllListeners();
    UE_LOG(LogDediKick, Log, TEXT("[DediKick] listen port=%u route=%s"),
        BoundPort, NetworkEndpoints::Dedi::Internal::KickPlayerRoutePattern());
}

void UDediKickEndpointSubsystem::Deinitialize()
{
    if (Router.IsValid() && RouteHandle.IsValid())
    {
        Router->UnbindRoute(RouteHandle);
    }
    RouteHandle.Reset();
    Router.Reset();

    // 다른 모듈도 같은 Module 을 공유하므로 StopAllListeners 는 호출하지 않는다.
    // (현재 프로젝트에는 다른 사용처 없음. 추가될 경우를 위해 보수적 결정.)

    Super::Deinitialize();
}

bool UDediKickEndpointSubsystem::HandleKickPlayer(
    const FHttpServerRequest& Request,
    const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete)
{
    // 1) loopback gate
    if (!IsLoopback(Request.PeerAddress))
    {
        const FString PeerStr = Request.PeerAddress.IsValid()
            ? Request.PeerAddress->ToString(true) : TEXT("(null)");
        UE_LOG(LogDediKick, Warning, TEXT("[DediKick] non-loopback rejected peer=%s"), *PeerStr);
        OnComplete(JsonResponse(403, TEXT("{\"error\":\"forbidden\"}")));
        return true;
    }

    // 2) Path param 에서 SteamID 추출
    const FString* SteamIdPtr = Request.PathParams.Find(NetworkEndpoints::Dedi::Internal::KickPlayerSteamIdParam());
    if (SteamIdPtr == nullptr || SteamIdPtr->IsEmpty())
    {
        OnComplete(JsonResponse(400, TEXT("{\"error\":\"missing_steam_id\"}")));
        return true;
    }
    const FString SteamId = *SteamIdPtr;

    // 3) Body 파싱 — reason 만. 빈 body 도 허용 (기본값 사용).
    FString Reason;
    if (Request.Body.Num() > 0)
    {
        const FString Body(reinterpret_cast<const char*>(Request.Body.GetData()), Request.Body.Num());
        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            Json->TryGetStringField(TEXT("reason"), Reason);
        }
    }
    // 기본 사유는 클라 모달에 그대로 표출되므로 사용자 가독 한국어로.
    // 안티치트 채널의 dual_kick reason("probe tamper"/"watchdog ws lost")이
    // body 에 동봉돼 오면 그쪽이 우선 (운영 진단용으로 영문 유지 의도면 호출처에서 한국어로 변환).
    if (Reason.IsEmpty()) Reason = TEXT("비인가 프로그램 사용");

    // 4) PC 찾기 → KickPlayer. 미발견은 200 (idempotent — 이미 Logout 처리됐을 수 있음)
    APlayerController* PC = FindControllerBySteamId(SteamId);
    if (PC == nullptr)
    {
        UE_LOG(LogDediKick, Log, TEXT("[DediKick] steam_id=%s 매칭 PC 없음 (idempotent ok)"), *SteamId);
        OnComplete(JsonResponse(200, TEXT("{\"ok\":true,\"kicked\":false}")));
        return true;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGameModeBase* GM = World ? World->GetAuthGameMode() : nullptr;
    AGameSession* Session = GM ? GM->GameSession : nullptr;
    if (Session == nullptr)
    {
        UE_LOG(LogDediKick, Error, TEXT("[DediKick] GameSession 없음 — kick skip"));
        OnComplete(JsonResponse(500, TEXT("{\"error\":\"no_session\"}")));
        return true;
    }

    const FText KickText = FText::FromString(Reason);
    Session->KickPlayer(PC, KickText);
    UE_LOG(LogDediKick, Log, TEXT("[DediKick] kicked steam_id=%s reason=%s"), *SteamId, *Reason);
    OnComplete(JsonResponse(200, TEXT("{\"ok\":true,\"kicked\":true}")));
    return true;
}

APlayerController* UDediKickEndpointSubsystem::FindControllerBySteamId(const FString& SteamId) const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World == nullptr) return nullptr;

    AGameStateBase* GS = World->GetGameState();
    if (GS == nullptr) return nullptr;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (PS == nullptr) continue;
        const FUniqueNetIdRepl& Id = PS->GetUniqueId();
        if (!Id.IsValid()) continue;
        if (Id->ToString() == SteamId)
        {
            return Cast<APlayerController>(PS->GetOwner());
        }
    }
    return nullptr;
}

#endif // WITH_SERVER_CODE
