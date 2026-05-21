// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Logging/LogMacros.h"
#include "SessionSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSessionSubsystem, Log, All);


// UI에서 결과를 알 수 있도록 블루프린트 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreateSessionResult, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinSessionResult, bool, bWasSuccessful);


// 방 가시성 모드 (Steam 로비 노출 정책)
UENUM(BlueprintType)
enum class ERoomVisibility : uint8
{
    Public      UMETA(DisplayName = "Public"),
    FriendsOnly UMETA(DisplayName = "Friends Only")
};


/**
 * 스팀 세션 관리용 서브시스템
 * GameInstance의 수명과 동일하게 자동 관리됩니다.
 */
UCLASS()
class INDIAN_BAB_API USessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    // 서브시스템 초기화 및 종료 (생성자/소멸자 역할)
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ----------------------------------------------------------------
    // 블루프린트에서 호출할 함수들 (UI 버튼과 연결)
    // ----------------------------------------------------------------

    // 방 생성 — Visibility=Public 이면 Steam 검색에 노출, FriendsOnly 면 친구 초대/Presence 합류만 허용
    UFUNCTION(BlueprintCallable, Category = "Session")
    void CreateRoom(int32 MaxPlayers, ERoomVisibility Visibility);

    // [Deprecated] 방 목록 위젯으로 교체 예정 — 현재는 즉시 실패 브로드캐스트
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinRoomByCode(FString InputCode);

    // 게임 시작 시점에 호스트가 자기 Steam Lobby를 잠금 (advertise/presence/invites off)
    // MainGameState OnRep_GamePhase(Starting)에서 모든 클라가 호출해도 안전 — 비호스트는 no-op
    UFUNCTION(BlueprintCallable, Category = "Session")
    void LockSessionForInGame();

    // 로비 복귀 시 광고 재개 (시나리오 B 인스턴스 재사용 대비)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void UnlockSessionForLobby();

    // ----------------------------------------------------------------
    // 델리게이트 (UI 이벤트 바인딩용)
    // ----------------------------------------------------------------
    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnCreateSessionResult OnCreateSessionCompleteEvent;

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnJoinSessionResult OnJoinSessionCompleteEvent;

    // 매치메이커 응답 후 클라이언트가 사용할 데디 endpoint를 SessionSettings에 심는 키
    static const FName Key_DediEndpoint;

    // 게임 진행 중 여부 (0=로비/대기, 1=게임 시작 후). 방 목록 위젯이 읽어 클릭 비활성화에 사용.
    static const FName Key_GameStarted;

protected:
    // 내부적으로 사용할 변수들
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<class FOnlineSessionSearch> SessionSearch;

    // 클라이언트: Join 성공 시 사용할 데디 URL (검색결과 SessionSettings에서 추출)
    FString PendingDediURL;

    // 호스트 여부 — Lock/UnlockSession 가드. CreateRoom 성공 시 true, Destroy/실패 시 false.
    bool bIsLocalHost = false;

    // 마지막 CreateRoom의 Visibility — UnlockSessionForLobby에서 원상복구용
    ERoomVisibility LastVisibility = ERoomVisibility::Public;

    // ----------------------------------------------------------------
    // Initialize 분기 헬퍼 — PIE / Standalone·패키지 두 흐름을 명확히 분리
    // ----------------------------------------------------------------
    void InitializeForEditor();    // PIE: Steam 미강제, 진단 로그만
    void InitializeForRuntime();   // Standalone/패키지: Steam 강제, 실패 시 종료

    // ----------------------------------------------------------------
    // 내부 콜백 함수 (OnlineSubsystem으로부터 응답을 받음)
    // ----------------------------------------------------------------
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    // ----------------------------------------------------------------
    // 매치메이커 흐름 (호스트 측)
    // ----------------------------------------------------------------
    // Steam Session 생성 직후 매치메이커에 인스턴스 생성 요청
    void RequestMatchmakerCreateInstance();
    // 매치메이커 응답 처리 (실제 HTTP 콜백 + 모킹 양쪽에서 호출)
    void HandleMatchmakerResponse(const FString& JsonBody, bool bOk);
    // 데디 URL 확정 후 SessionSettings 갱신 + 호스트 본인 ClientTravel
    void FinalizeHostTravel(const FString& DediURL);

    // 델리게이트 핸들 (바인딩 해제용)
    FDelegateHandle CreateSessionCompleteDelegateHandle;
    FDelegateHandle FindSessionsCompleteDelegateHandle;
    FDelegateHandle JoinSessionCompleteDelegateHandle;
	
};
