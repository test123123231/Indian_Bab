#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRouteHandle.h"
#include "DediKickEndpointSubsystem.generated.h"

class IHttpRouter;
struct FHttpServerRequest;
struct FHttpServerResponse;

/**
 * 데디 전용. AntiCheat 가 발사하는 POST /internal/dedi/kick_player 를 수신해
 * 해당 SteamID 의 PlayerController 를 KickPlayer 로 추방한다.
 *
 * 포트 정책: 게임 UDP 포트(7777~7876) + 10000 (17777~17876). 게임 포트는 외부
 * 공개라 같은 포트 번호의 TCP 에 HTTP 를 얹으면 핸들러 자체가 외부에서 보임.
 * 별도 대역으로 분리해 운영 방화벽에서 17xxx 를 닫아두는 것이 SSoT.
 *
 * 그 위에 핸들러는 PeerAddress 가 loopback 인지 검증해 다중 방어선 유지.
 * loopback 외 요청은 403.
 *
 * 라이프사이클: GameInstance Initialize 시 라우터 바인드 + 리스너 기동.
 * Deinitialize 시 언바인드 + 정지. ShouldCreateSubsystem 은 데디만 true.
 */
UCLASS()
class INDIAN_BAB_API UDediKickEndpointSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 게임 UDP 포트(-port=) 에서 HTTP 포트(+10000) 를 파생. CLI 미주입 시 7777 기준. */
    static uint16 ResolveHttpPort();

private:
    bool HandleKickPlayer(const FHttpServerRequest& Request,
                          const TFunction<void(TUniquePtr<FHttpServerResponse>&&)>& OnComplete);

    /** GameState->PlayerArray 순회. 매칭된 PC 반환. 없으면 nullptr. */
    class APlayerController* FindControllerBySteamId(const FString& SteamId) const;

    TSharedPtr<IHttpRouter> Router;
    FHttpRouteHandle RouteHandle;
    uint16 BoundPort = 0;
};
