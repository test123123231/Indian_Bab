// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Logging/LogMacros.h"
#include "SessionSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSessionSubsystem, Log, All);


// UI에서 결과를 알 수 있도록 블루프린트 델리게이트 선언
//   - JoinResult: Join 시도 결과 (성공 시 위젯 닫기, 실패 시 버튼 재활성화)
//   - SessionError: 모든 에러 사유 텍스트 단일 채널 (매치메이커 실패 / Create 실패 / 데디 거부 / Join 가드 실패)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinSessionResult, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionError, const FString&, Reason);


// 방 가시성 모드 (Steam 로비 노출 정책)
UENUM(BlueprintType)
enum class ERoomVisibility : uint8
{
    Public      UMETA(DisplayName = "Public"),
    FriendsOnly UMETA(DisplayName = "Friends Only")
};


// 방 목록 위젯에 노출할 행 데이터 (FOnlineSessionSearchResult는 USTRUCT 아니라 BP 노출 불가 → 어댑터)
USTRUCT(BlueprintType)
struct FRoomListEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Session") FString HostName;
    UPROPERTY(BlueprintReadOnly, Category = "Session") int32 CurrentPlayers = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Session") int32 MaxPlayers = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Session") bool bGameStarted = false;
    UPROPERTY(BlueprintReadOnly, Category = "Session") bool bFriendsOnly = false;

    // SessionSearch->SearchResults 인덱스. JoinRoomByIndex에 그대로 넘긴다.
    UPROPERTY(BlueprintReadOnly, Category = "Session") int32 SearchResultIndex = INDEX_NONE;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomsFoundResult, bool, bWasSuccessful, const TArray<FRoomListEntry>&, Rooms);


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

    // 방 목록 검색 — Public + FriendsOnly(친구 로비만 Steam이 반환) 통합. 결과는 OnRoomsFoundEvent로.
    UFUNCTION(BlueprintCallable, Category = "Session")
    void FindRooms();

    // 검색결과 인덱스 기반 참가. FindRooms 응답으로 받은 FRoomListEntry.SearchResultIndex 그대로 넘긴다.
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinRoomByIndex(int32 SearchResultIndex);

    // 게임 시작 시점에 호스트가 자기 Steam Lobby를 잠금 (advertise/presence/invites off)
    // MainGameState OnRep_GamePhase(Starting)에서 모든 클라가 호출해도 안전 — 비호스트는 no-op
    UFUNCTION(BlueprintCallable, Category = "Session")
    void LockSessionForInGame();

    // 로비 복귀 시 광고 재개 (시나리오 B 인스턴스 재사용 대비)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void UnlockSessionForLobby();

    // ----------------------------------------------------------------
    // 델리게이트 (UI 이벤트 바인딩용) — 통합 후 3개
    // ----------------------------------------------------------------
    // Join 시도 결과 (성공/실패) — 위젯이 성공 시 UI 닫기/실패 시 버튼 재활성화
    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnJoinSessionResult OnJoinSessionCompleteEvent;

    // 방 검색 결과
    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnRoomsFoundResult OnRoomsFoundEvent;

    // 모든 에러 사유 텍스트 단일 채널:
    //   - 매치메이커 HTTP/파싱/필드 누락
    //   - CreateSession OSS 실패
    //   - 데디 PreLogin 거부 (GameInstance::OnNetworkFailure → CleanupHostSession 경유)
    //   - JoinRoom 가드 실패 / JoinSession OSS 실패
    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnSessionError OnSessionErrorEvent;

    // 매치메이커 실패 / 데디 거부 공통 정리:
    //   - 호스트(bIsLocalHost=true): DestroySession → 매치메이커 응답 전 만든 Steam Lobby 폐기, 유령 방 차단
    //   - 클라(bIsLocalHost=false): DestroySession → 호스트 로비에서 leave, 호스트 측 슬롯 즉시 반환
    // 두 경우 모두 OnSessionErrorEvent로 사유 broadcast.
    UFUNCTION(BlueprintCallable, Category = "Session")
    void CleanupHostSession(const FString& Reason);

    // 매치메이커 응답 후 클라이언트가 사용할 데디 endpoint를 SessionSettings에 심는 키
    static const FName Key_DediEndpoint;

    // 게임 진행 중 여부 (0=로비/대기, 1=게임 시작 후). 방 목록 위젯이 읽어 클릭 비활성화에 사용.
    static const FName Key_GameStarted;

    // 게임 식별 태그 — AppID 480(Spacewar) 공용 풀에서 Indian_Bab 로비만 골라내기 위한 필터 키.
    // Create 시 박제하고 Find 시 Equals 필터로 사용.
    // int32 magic 사용 사유: UE Steam OSS의 string filter는 metadata type-prefix encoding으로 매칭 실패 빈발.
    //                       numerical filter는 prefix 처리 경로가 단순해 안정적.
    static const FName Key_GameTag;
    static const int32 GameTagMagic;

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

    // DestroySession 완료 콜백 — CleanupHostSession 공용 (호스트/클라 분기는 캡처된 플래그로 처리)
    void OnDestroySessionAfterCleanup(FName SessionName, bool bWasSuccessful);

    FDelegateHandle DestroySessionCompleteDelegateHandle;

    // CleanupHostSession → DestroySession 비동기 완료 콜백 사이의 임시 보관 (한 번에 한 흐름만)
    FString PendingCleanupReason;

    // 델리게이트 핸들 (바인딩 해제용)
    FDelegateHandle CreateSessionCompleteDelegateHandle;
    FDelegateHandle FindSessionsCompleteDelegateHandle;
    FDelegateHandle JoinSessionCompleteDelegateHandle;
	
};
