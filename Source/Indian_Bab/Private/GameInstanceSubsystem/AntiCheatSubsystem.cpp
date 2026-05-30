#include "GameInstanceSubsystem/AntiCheatSubsystem.h"

#include "AntiCheat/WatchdogLoader.h"
#include "Network/NetworkEndpoints.h"
#include "GameInstanceSubsystem/SteamCredentialsSubsystem.h"

#include "Engine/GameInstance.h"

#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Ticker.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#endif

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogAntiCheat, Log, All);

namespace
{
    // SteamCredentialsSubsystem의 FatalExitSteamMissing과 동일한 패턴 —
    // Windows MessageBox로 사유 안내 후 RequestExit. PIE/에디터 분기 없음(서브시스템
    // 자체가 Standalone에서만 도달 가능: ANTICHEAT_USE_MOCK이 에디터에서 ShouldCreateSubsystem 차단).
    void FatalExitWithDialog(const TCHAR* Title, const TCHAR* Body, const TCHAR* LogReason)
    {
#if PLATFORM_WINDOWS
        ::MessageBoxW(nullptr, Body, Title, MB_OK | MB_ICONERROR);
#endif
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] %s. 게임 종료."), LogReason);
        // bForce=true → 내부적으로 TerminateProcess 호출. 부팅 가드(GameInstance::Init
        // 도중) 단계라 graceful 종료(bForce=false) 시 엔진이 메인 루프에 진입하기 전
        // 종료 플래그 확인 지점이 늦거나 도달 안 해서 프로세스가 안 죽는 케이스가 있음.
        // 모든 호출 사이트는 워치독 시작 전이라 force 종료에 따른 cleanup 손실 없음.
        FPlatformMisc::RequestExit(true);
    }
}

bool UAntiCheatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if ANTICHEAT_USE_MOCK
    // AC 서버 미사용 모드 — 서브시스템 자체 생성 X (Initialize/StartVerification 도달 불가)
    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] ANTICHEAT_USE_MOCK — AntiCheatSubsystem 생성 스킵"));
    return false;
#else
    // 데디 서버는 안티치트 클라이언트 검증을 수행하지 않음 (verify는 클라 부팅 단계 책임)
    if (IsRunningDedicatedServer())
    {
        return false;
    }
    return Super::ShouldCreateSubsystem(Outer);
#endif
}

void UAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // SteamCredentialsSubsystem이 부팅 가드 + ticket SSoT — 우리는 그 위에 올라탐.
    // 의존성 선언으로 SteamCreds가 먼저 init되도록 보장 (서브시스템 등록 순서 비결정성 회피).
    Collection.InitializeDependency(USteamCredentialsSubsystem::StaticClass());
    Super::Initialize(Collection);

    StartVerification();

    // SteamCredentialsSubsystem 부팅 가드와 동일한 UX를 위해 verify 완료까지
    // 동기 대기 — 비동기 콜백(Steam ticket·HTTP 응답)을 수동 펌프하여 메인메뉴 로드 전에
    // 결과 모달을 띄운다. StartVerification 자체가 동기 실패해 RequestExit한 경우
    // PumpUntilVerifyComplete가 첫 체크에서 즉시 리턴.
    PumpUntilVerifyComplete();
}

void UAntiCheatSubsystem::Deinitialize()
{
    WatchdogLoader::Unload(WatchdogHandle);
    Super::Deinitialize();
}

void UAntiCheatSubsystem::StartVerification()
{
    const FString ExeHash = ComputeExeSha256();
    if (ExeHash.IsEmpty())
    {
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 초기화 실패"),
            TEXT("실행 파일 해시 계산에 실패했습니다.\n게임을 종료합니다."),
            TEXT("exe hash 계산 실패"));
        return;
    }
    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] exe_hash=%s"), *ExeHash);

    USteamCredentialsSubsystem* SteamCreds =
        GetGameInstance() ? GetGameInstance()->GetSubsystem<USteamCredentialsSubsystem>() : nullptr;
    if (!SteamCreds)
    {
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 초기화 실패"),
            TEXT("Steam 자격 증명 서브시스템을 찾을 수 없습니다.\n게임을 종료합니다."),
            TEXT("SteamCredentialsSubsystem 없음"));
        return;
    }

    TWeakObjectPtr<UAntiCheatSubsystem> WeakThis(this);
    SteamCreds->RequestTicket(
        [WeakThis, ExeHash](bool bOk, FString TicketHex, FString SteamId)
        {
            if (!WeakThis.IsValid()) return;
            if (!bOk)
            {
                FatalExitWithDialog(
                    TEXT("Indian_Bab — 안티치트 초기화 실패"),
                    TEXT("Steam 인증 티켓 발급에 실패했습니다.\nSteam 클라이언트 실행 상태를 확인하세요.\n게임을 종료합니다."),
                    TEXT("Steam 인증 정보 획득 실패"));
                return;
            }
            UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] steam ticket received (len=%d), steam_id=%s"),
                TicketHex.Len(), *SteamId);
            WeakThis->CachedSteamId = SteamId;
            WeakThis->SendVerifyRequest(ExeHash, TicketHex, SteamId);
        });
}

FString UAntiCheatSubsystem::ComputeExeSha256() const
{
    const FString ExePath = FPlatformProcess::ExecutablePath();

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *ExePath))
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] cannot read exe at %s"), *ExePath);
        return FString();
    }

#if PLATFORM_WINDOWS
    BCRYPT_ALG_HANDLE AlgHandle = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&AlgHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] BCryptOpenAlgorithmProvider failed"));
        return FString();
    }

    BCRYPT_HASH_HANDLE HashHandle = nullptr;
    NTSTATUS Status = BCryptCreateHash(AlgHandle, &HashHandle, nullptr, 0, nullptr, 0, 0);
    if (!NT_SUCCESS(Status))
    {
        BCryptCloseAlgorithmProvider(AlgHandle, 0);
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] BCryptCreateHash failed (0x%08x)"), Status);
        return FString();
    }

    Status = BCryptHashData(HashHandle,
        const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(Bytes.GetData())),
        static_cast<ULONG>(Bytes.Num()), 0);

    uint8 Digest[32] = {};
    if (NT_SUCCESS(Status))
    {
        Status = BCryptFinishHash(HashHandle, Digest, sizeof(Digest), 0);
    }

    BCryptDestroyHash(HashHandle);
    BCryptCloseAlgorithmProvider(AlgHandle, 0);

    if (!NT_SUCCESS(Status))
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] BCrypt SHA256 failed (0x%08x)"), Status);
        return FString();
    }
    return BytesToHex(Digest, 32).ToLower();
#else
    UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] SHA256 not implemented on this platform"));
    return FString();
#endif
}

void UAntiCheatSubsystem::SendVerifyRequest(const FString& ExeHashHex,
                                            const FString& SteamTicketHex,
                                            const FString& SteamId)
{
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("exe_hash"), ExeHashHex);
    Body->SetStringField(TEXT("steam_auth_ticket"), SteamTicketHex);
    Body->SetStringField(TEXT("steam_id"), SteamId);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(NetworkEndpoints::AC::External::Verify());
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(Payload);

    TWeakObjectPtr<UAntiCheatSubsystem> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!WeakThis.IsValid()) return;
            const FString RespBody = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
            WeakThis->HandleVerifyResponse(RespBody, Code, bOk);
        });
    Req->ProcessRequest();
}

void UAntiCheatSubsystem::HandleVerifyResponse(const FString& Body, int32 StatusCode, bool bConnectionOk)
{
    if (!bConnectionOk)
    {
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 서버 연결 실패"),
            TEXT("안티치트 서버에 연결할 수 없습니다.\n서버가 실행 중인지 확인하세요.\n게임을 종료합니다."),
            TEXT("verify 요청 실패 (연결 오류)"));
        return;
    }

    if (StatusCode != 200)
    {
        const FString Detail = FString::Printf(
            TEXT("안티치트 서버 검증에 실패했습니다.\n상태 코드: %d\n게임을 종료합니다."),
            StatusCode);
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] verify body=%s"), *Body);
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 검증 실패"),
            *Detail,
            *FString::Printf(TEXT("verify 실패 status=%d"), StatusCode));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] verify body=%s"), *Body);
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 응답 오류"),
            TEXT("안티치트 서버 응답을 해석할 수 없습니다.\n게임을 종료합니다."),
            TEXT("verify response parse error"));
        return;
    }

    SessionKey = Json->GetStringField(TEXT("session_key"));
    DllKey     = Json->GetStringField(TEXT("dll_key"));
    bVerified  = !SessionKey.IsEmpty() && !DllKey.IsEmpty();

    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] verified (session_key+dll_key received)"));

    if (!bVerified)
    {
        FatalExitWithDialog(
            TEXT("Indian_Bab — 안티치트 응답 오류"),
            TEXT("안티치트 서버 응답에 세션 정보가 누락되었습니다.\n게임을 종료합니다."),
            TEXT("verify response missing session_key/dll_key"));
        return;
    }

    LaunchWatchdog();
}

void UAntiCheatSubsystem::LaunchWatchdog()
{
    const FString WdPath = FPaths::ProjectContentDir() / TEXT("NonUFS/wd.dat");
    TArray<uint8> EncBlob;
    if (!FFileHelper::LoadFileToArray(EncBlob, *WdPath))
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] wd.dat 경로: %s"), *WdPath);
        FatalExitWithDialog(
            TEXT("Indian_Bab — 워치독 페이로드 누락"),
            TEXT("워치독 암호화 페이로드(wd.dat)를 찾을 수 없습니다.\n게임을 종료합니다."),
            TEXT("wd.dat 로드 실패"));
        return;
    }

    TArray<uint8> Key;
    if (!FBase64::Decode(DllKey, Key) || Key.Num() != 32)
    {
        FMemory::Memzero(Key.GetData(), Key.Num());
        FatalExitWithDialog(
            TEXT("Indian_Bab — 워치독 키 오류"),
            TEXT("워치독 복호화 키가 올바르지 않습니다.\n게임을 종료합니다."),
            TEXT("dll_key 디코드 실패"));
        return;
    }

    FWatchdogStartParams Params;
    Params.UserId        = CachedSteamId;
    Params.SessionKeyB64 = SessionKey;
    // 워치독 WS는 AC 서버와 동일 host/port — NetworkEndpoints::AC가 노출하는 host/port/tls 그대로.
    // 운영 변경은 Indian_Bab.Build.cs의 ACHost/ACPort/ACUseTls 한 곳만 수정.
    Params.Host          = NetworkEndpoints::AC::WsHost();
    Params.Port          = NetworkEndpoints::AC::WsPort();
    Params.bUseTls       = NetworkEndpoints::AC::WsUseTls();

    const bool bOk = WatchdogLoader::LoadAndStart(EncBlob, Key, Params, WatchdogHandle);

    // 키/평문 페이로드 zeroize — 메모리 잔류 최소화.
    FMemory::Memzero(Key.GetData(), Key.Num());
    Key.Empty();
    FMemory::Memzero(EncBlob.GetData(), EncBlob.Num());
    EncBlob.Empty();
    DllKey.Empty();

    if (!bOk)
    {
        FatalExitWithDialog(
            TEXT("Indian_Bab — 워치독 로딩 실패"),
            TEXT("워치독 모듈 복호화 또는 매핑에 실패했습니다.\n페이로드가 손상되었거나 변조되었을 수 있습니다.\n게임을 종료합니다."),
            TEXT("워치독 로딩 실패"));
        return;
    }
}

void UAntiCheatSubsystem::PumpUntilVerifyComplete()
{
    // SteamCreds 부팅 가드의 동기 모달과 동일한 UX를 만들기 위해 Initialize에서
    // 게임스레드를 점유하며 콜백을 펌프한다. 완료 조건은 셋 중 하나:
    //   (a) WatchdogHandle != nullptr  → verify+매핑 성공.
    //   (b) IsEngineExitRequested()    → 어느 FatalExitWithDialog가 이미 호출됨.
    //   (c) 데드라인 초과              → FatalExitWithDialog로 동일하게 종료.
    // 펌프 대상:
    //   - FHttpManager::Tick : verify HTTP 응답 콜백 디스패치.
    //   - FTSTicker (Core)   : SteamCreds 5s 타임아웃 ticker.
    //   - SteamAPI_RunCallbacks : GetAuthSessionTicketResponse_t 콜백.
    constexpr double DeadlineSeconds = 20.0;
    constexpr float  StepSeconds     = 0.01f;
    const double Start = FPlatformTime::Seconds();

    while (!IsEngineExitRequested() && WatchdogHandle == nullptr)
    {
        FHttpModule::Get().GetHttpManager().Tick(StepSeconds);
        FTSTicker::GetCoreTicker().Tick(StepSeconds);
        SteamAPI_RunCallbacks();

        if (FPlatformTime::Seconds() - Start > DeadlineSeconds)
        {
            FatalExitWithDialog(
                TEXT("Indian_Bab — 안티치트 초기화 타임아웃"),
                TEXT("안티치트 초기화가 시간 내에 완료되지 않았습니다.\n네트워크 또는 Steam 상태를 확인하세요.\n게임을 종료합니다."),
                TEXT("verify 펌프 데드라인 초과"));
            return;
        }
        FPlatformProcess::Sleep(StepSeconds);
    }
}
