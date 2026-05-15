#include "GameInstanceSubsystem/AntiCheatSubsystem.h"

#include "Network/AntiCheatConfig.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAntiCheat, Log, All);

void UAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

#if UE_BUILD_SHIPPING
    // 단계 A 부팅 가드(인터넷 미연결 시 종료)는 ConnectivitySubsystem::Initialize 가 단독으로 담당.
    // 여기서는 verify 흐름만 실행.
    StartVerification();
#else
    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] Non-Shipping build — 초기화 스킵"));
#endif
}

void UAntiCheatSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UAntiCheatSubsystem::StartVerification()
{
    const FString ExeHash = ComputeExeSha256();
    if (ExeHash.IsEmpty())
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] exe hash 계산 실패. 게임 종료."));
        FPlatformMisc::RequestExit(false);
        return;
    }
    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] exe_hash=%s"), *ExeHash);

    FString TicketHex;
    FString SteamId;
    if (!TryGetSteamCredentials(TicketHex, SteamId))
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] Steam 인증 정보 획득 실패. 게임 종료."));
        FPlatformMisc::RequestExit(false);
        return;
    }
    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] steam ticket received (len=%d), steam_id=%s"),
        TicketHex.Len(), *SteamId);

    SendVerifyRequest(ExeHash, TicketHex, SteamId);
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

bool UAntiCheatSubsystem::TryGetSteamCredentials(FString& OutTicketHex, FString& OutSteamId) const
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS)
    {
        UE_LOG(LogAntiCheat, Warning, TEXT("[AntiCheat] OnlineSubsystem not available"));
        return false;
    }

    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        UE_LOG(LogAntiCheat, Warning, TEXT("[AntiCheat] Identity interface invalid"));
        return false;
    }

    constexpr int32 LocalUserNum = 0;

    // Steam OSS 의 GetAuthToken 은 Steam Auth Session Ticket 의 hex 문자열을 동기 반환한다.
    OutTicketHex = Identity->GetAuthToken(LocalUserNum);
    if (OutTicketHex.IsEmpty())
    {
        UE_LOG(LogAntiCheat, Warning, TEXT("[AntiCheat] auth token empty (Steam not logged in?)"));
        return false;
    }

    FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(LocalUserNum);
    if (!NetId.IsValid())
    {
        UE_LOG(LogAntiCheat, Warning, TEXT("[AntiCheat] unique net id invalid"));
        return false;
    }
    OutSteamId = NetId->ToString();
    return true;
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
    Req->SetURL(AntiCheatConfig::BaseURL + AntiCheatConfig::VerifyPath);
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
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] verify 요청 실패 (연결 오류). 게임 종료."));
        FPlatformMisc::RequestExit(false);
        return;
    }

    if (StatusCode != 200)
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] verify 실패: status=%d body=%s. 게임 종료."), StatusCode, *Body);
        FPlatformMisc::RequestExit(false);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogAntiCheat, Error, TEXT("[AntiCheat] verify response parse error: %s"), *Body);
        bVerified = false;
        return;
    }

    Token      = Json->GetStringField(TEXT("token"));
    SessionKey = Json->GetStringField(TEXT("session_key"));
    DllKey     = Json->GetStringField(TEXT("dll_key"));
    ExpiresAt  = static_cast<int64>(Json->GetNumberField(TEXT("expires_at")));
    bVerified  = !Token.IsEmpty();

    UE_LOG(LogAntiCheat, Log, TEXT("[AntiCheat] verified token=%s expires_at=%lld"),
        *Token, ExpiresAt);
}
