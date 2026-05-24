#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SteamCredentialsSubsystem.generated.h"

/**
 * Steam Auth Session Ticket 비동기 발급 서브시스템 (클라 전용).
 *
 * GetAuthSessionTicket은 호출 즉시 hex를 반환하지만, Steam 백엔드가 그 티켓을
 * ISteamUserAuth/AuthenticateUserTicket에서 유효한 것으로 인정하는 시점은
 * GetAuthSessionTicketResponse_t 콜백이 OK로 떨어진 다음이다.
 * 동기 반환은 errorcode 101(Invalid ticket)을 유발하므로, 콜백을 기다린 뒤
 * 결과를 callback으로 전달한다.
 *
 * 데디 서버는 ShouldCreateSubsystem에서 false → 인스턴스 미생성.
 * OSS Steam 미초기화/Steam 미로그인 시 Initialize에서 bAvailable=false로
 * 캐시되고, 이후 RequestTicket은 즉시 실패 콜백.
 */
UCLASS()
class INDIAN_BAB_API USteamCredentialsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // bSuccess=true → TicketHex(uppercase), SteamId 채워짐.
    // 실패(서브시스템 unavailable / Steam 미로그인 / result!=OK / 5s 타임아웃) → 둘 다 비어있음.
    using FCallback = TFunction<void(bool bSuccess, FString TicketHex, FString SteamId)>;

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void RequestTicket(FCallback Callback);

    bool IsAvailable() const { return bAvailable; }

private:
    void InitializeForEditor();
    void InitializeForRuntime();

    bool bAvailable = false;
};
