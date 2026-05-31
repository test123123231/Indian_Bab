#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AntiCheatSubsystem.generated.h"

/**
 * 게임 클라이언트 부팅 시 안티치트 서버에 검증 요청을 보내는 서브시스템.
 * POST /api/verify 호출 후 session_key / dll_key 를 캐싱한다.
 * 세션 식별은 SteamID(SteamCredentialsSubsystem에서 회수)로 통일 — ACToken 보관 안 함.
 * 워치독 WebSocket 연결은 후속 작업에서 추가.
 */
UCLASS()
class INDIAN_BAB_API UAntiCheatSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "AntiCheat")
    bool IsVerified() const { return bVerified; }

    const FString& GetSessionKey() const { return SessionKey; }
    const FString& GetDllKey() const     { return DllKey; }

private:
    void StartVerification();

    // EXE 파일 SHA-256 해시(64자 lowercase hex). 부팅 시 1회 계산.
    FString ComputeExeSha256() const;

    void SendVerifyRequest(const FString& ExeHashHex,
                           const FString& SteamTicketHex,
                           const FString& SteamId);

    void HandleVerifyResponse(const FString& Body, int32 StatusCode, bool bConnectionOk);

    void LaunchWatchdog();

    // 비동기 verify·LaunchWatchdog가 끝날 때까지 게임스레드 동기 펌프.
    // SteamCreds 부팅 가드와 동일하게 메인메뉴 로드 전에 결과 모달이 뜨도록.
    void PumpUntilVerifyComplete();

    FString SessionKey;     // base64 그대로 보관 (HMAC 사용 시 FBase64::Decode)
    FString DllKey;         // base64 32B. 워치독 복호화 직후 Empty() 처리 (메모리 잔류 최소화)
    FString CachedSteamId;  // verify 콜백에서 받은 SteamID — 워치독 컨텍스트 user_id
    bool    bVerified = false;

    // 수동 매핑된 워치독 인스턴스 핸들 (WatchdogLoader 내부 타입). Deinitialize에서 Unload.
    void* WatchdogHandle = nullptr;
};
