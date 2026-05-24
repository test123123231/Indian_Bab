#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AntiCheatSubsystem.generated.h"

/**
 * 게임 클라이언트 부팅 시 안티치트 서버에 검증 요청을 보내는 서브시스템.
 * POST /api/verify 호출 후 token / session_key / dll_key / expires_at 을 캐싱한다.
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

    const FString& GetToken() const      { return Token; }
    const FString& GetSessionKey() const { return SessionKey; }
    const FString& GetDllKey() const     { return DllKey; }
    int64 GetExpiresAt() const           { return ExpiresAt; }

private:
    void StartVerification();

    // EXE 파일 SHA-256 해시(64자 lowercase hex). 부팅 시 1회 계산.
    FString ComputeExeSha256() const;

    void SendVerifyRequest(const FString& ExeHashHex,
                           const FString& SteamTicketHex,
                           const FString& SteamId);

    void HandleVerifyResponse(const FString& Body, int32 StatusCode, bool bConnectionOk);

    FString Token;
    FString SessionKey;   // base64 그대로 보관 (HMAC 사용 시 FBase64::Decode)
    FString DllKey;       // base64 그대로 보관, 메모리에만 둔다
    int64   ExpiresAt = 0;
    bool    bVerified = false;
};
