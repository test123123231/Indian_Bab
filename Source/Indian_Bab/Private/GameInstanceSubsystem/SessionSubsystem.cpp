#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/LocalPlayer.h"
#include "Network/DediServerConfig.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif


DEFINE_LOG_CATEGORY(LogSessionSubsystem);

#define MATCHMAKER_USE_MOCK 1

const FName USessionSubsystem::Key_DediEndpoint = FName("DediEndpoint");
const FName USessionSubsystem::Key_GameStarted = FName("GameStarted");
const FName USessionSubsystem::Key_GameTag = FName("INDIANBABID");
const int32 USessionSubsystem::GameTagMagic = 0x1B7A;  // 'IndianBab' magic — Steam pool 충돌 회피용 임의값


namespace
{
    void FatalExitSteamMissing(const TCHAR* Reason)
    {
#if PLATFORM_WINDOWS
        ::MessageBoxW(nullptr,
            L"Steam 클라이언트가 실행 중인지 확인하세요.\n게임을 종료합니다.",
            L"Indian_Bab — Steam 초기화 실패",
            MB_OK | MB_ICONERROR);
#endif
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem] Steam init failed — %s. Exiting."), Reason);
        FPlatformMisc::RequestExit(false);
    }
}


void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem] 세션 테스트 로그"));
    // GIsEditor: PIE 자식 월드에선 true, Standalone 자식 프로세스·패키지 빌드에선 false.
    // → PIE만 우회, Standalone과 패키지는 Steam 강제.
    if (GIsEditor)
    {
        InitializeForEditor();
    }
    else
    {
        InitializeForRuntime();
    }
}


void USessionSubsystem::InitializeForEditor()
{
    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Editor] OnlineSubsystem 조회 시도."));

    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("[SessionSubsystem][Editor] IOnlineSubsystem::Get() == null — 세션 기능 비활성 상태로 계속 진행."));
        return;
    }

    const FName SubsystemName = Subsystem->GetSubsystemName();
    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Editor] OnlineSubsystem 이름: %s"), *SubsystemName.ToString());

    SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("[SessionSubsystem][Editor] SessionInterface 무효 — CreateRoom/JoinRoomByCode 호출 시 즉시 실패 브로드캐스트됨."));
        return;
    }

    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Editor] SessionInterface 확보 완료 (%s)."), *SubsystemName.ToString());
}


void USessionSubsystem::InitializeForRuntime()
{
    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Runtime] Steam 검사 시작."));

    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem][Runtime] IOnlineSubsystem::Get() == null."));
        FatalExitSteamMissing(TEXT("Steam not detected (OnlineSubsystem null)"));
        return;
    }

    const FName SubsystemName = Subsystem->GetSubsystemName();
    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Runtime] OnlineSubsystem 이름: %s (기대값: STEAM)"), *SubsystemName.ToString());

    if (SubsystemName != FName(TEXT("STEAM")))
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem][Runtime] OnlineSubsystem 이름이 STEAM이 아님: %s"), *SubsystemName.ToString());
        FatalExitSteamMissing(TEXT("Steam not detected (subsystem name mismatch)"));
        return;
    }

    SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem][Runtime] GetSessionInterface() 반환값 무효."));
        FatalExitSteamMissing(TEXT("Steam session interface unavailable"));
        return;
    }

    UE_LOG(LogSessionSubsystem, Log, TEXT("[SessionSubsystem][Runtime] Steam SessionInterface 확보 완료."));
}


void USessionSubsystem::Deinitialize()
{
    Super::Deinitialize();
}


// ---------------------------------------------------------
// 방 생성
// ---------------------------------------------------------
void USessionSubsystem::CreateRoom(int32 MaxPlayers, ERoomVisibility Visibility)
{
    if (!SessionInterface.IsValid())
    {
        OnSessionErrorEvent.Broadcast(TEXT("Steam 세션 인터페이스를 사용할 수 없습니다."));
        return;
    }

    // 이미 세션이 있다면 파괴 후 재생성 (안전장치)
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
        bIsLocalHost = false;
    }

    LastVisibility = Visibility;

    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnCreateSessionComplete));

    // 공통 설정 — Steam 로비 기반
    // 주의: Steam OSS의 BuildLobbyType은 bShouldAdvertise=false면 무조건 k_ELobbyTypePrivate 상태
    //       → Public/FriendsOnly 둘 다 bShouldAdvertise=true 유지, 구분은 PresenceFriendsOnly 플래그로
    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = false;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bUseLobbiesIfAvailable = true;
    // 진행 중 합류는 허용 — 옛 작동 코드와 정합. 게임 시작 후 차단은 LockSessionForInGame() + dedi PreLogin 백스톱이 담당.
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bAllowInvites = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.NumPublicConnections = MaxPlayers;

    // 가시성 분기 — Public: 누구나 검색/합류 / FriendsOnly: Steam 친구만 (k_ELobbyTypeFriendsOnly 매핑)
    const bool bPublic = (Visibility == ERoomVisibility::Public);
    SessionSettings.bAllowJoinViaPresence = bPublic;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = !bPublic;

    SessionSettings.Set(Key_GameStarted, (int32)0, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SessionSettings.Set(Key_GameTag, GameTagMagic, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("Creating Room: Visibility=%s, MaxPlayers=%d"),
            bPublic ? TEXT("Public") : TEXT("FriendsOnly"), MaxPlayers);
        SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings);
    }
}


void USessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    // 델리게이트 해제 (메모리 관리)
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
    }

    if (!bWasSuccessful)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Failed to Create Session."));
        bIsLocalHost = false;
        OnSessionErrorEvent.Broadcast(TEXT("Steam 세션 생성에 실패했습니다."));
        return;
    }

    bIsLocalHost = true;
    UE_LOG(LogSessionSubsystem, Warning, TEXT("Steam Session OK. Requesting matchmaker..."));
    RequestMatchmakerCreateInstance();
}


// ---------------------------------------------------------
// 매치메이커 흐름 (호스트)
// ---------------------------------------------------------
void USessionSubsystem::RequestMatchmakerCreateInstance()
{
    #if !MATCHMAKER_USE_MOCK
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(MatchmakerConfig::BaseURL + MatchmakerConfig::CreateMatchPath);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(TEXT("{}"));  // 추후 호스트 SteamID 등 주입
    // 매치메이커 다운/방화벽 차단 시 무한 대기 방지 — 15s 후 bOk=false로 콜백
    Req->SetTimeout(15.0f);
    Req->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            HandleMatchmakerResponse(
                Resp.IsValid() ? Resp->GetContentAsString() : FString(),
                bOk && Resp.IsValid() && Resp->GetResponseCode() == 200);
        });
    Req->ProcessRequest();
    return;
    #else
    const FString MockResponse = TEXT(R"({"match_id":"TEST_MATCH_001","host_ip":"127.0.0.1","port":7777})");
    HandleMatchmakerResponse(MockResponse, true);
    #endif
}


void USessionSubsystem::HandleMatchmakerResponse(const FString& JsonBody, bool bOk)
{
    if (!bOk)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker request failed (network/timeout/non-200). body=%s"), *JsonBody);
        CleanupHostSession(TEXT("매치메이커 서버에 연결할 수 없습니다. 잠시 후 다시 시도해주세요."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    auto Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker response parse failed: %s"), *JsonBody);
        CleanupHostSession(TEXT("매치메이커 응답 형식이 올바르지 않습니다."));
        return;
    }

    // 필수 필드 누락(데디 풀 고갈 등 매치메이커 에러 응답) 방어 — GetStringField는 누락 시 빈 문자열
    FString HostIp;
    int32 Port = 0;
    if (!Json->TryGetStringField(TEXT("host_ip"), HostIp) ||
        !Json->TryGetNumberField(TEXT("port"), Port) ||
        HostIp.IsEmpty() || Port <= 0)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker response missing host_ip/port. body=%s"), *JsonBody);
        CleanupHostSession(TEXT("사용 가능한 게임 서버가 없습니다. 잠시 후 다시 시도해주세요."));
        return;
    }

    const FString DediURL = FString::Printf(TEXT("%s:%d"), *HostIp, Port);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Matchmaker response → Dedi: %s"), *DediURL);
    FinalizeHostTravel(DediURL);
}


// ---------------------------------------------------------
// 매치메이커/데디 거부 공통 정리 (호스트·클라 통합)
// ---------------------------------------------------------
// IOnlineSession::DestroySession은 NamedSession 소유권에 따라 Steam OSS 내부에서 자동 분기:
//   - 호스트(Lobby Owner): FOnlineAsyncTaskSteamDestroyLobby → 로비 해체 + 멤버 전원 강제 이탈
//   - 클라(Lobby Member): FOnlineAsyncTaskSteamLeaveLobby → SteamMatchmaking()->LeaveLobby()
//     → 호스트가 LobbyChatUpdate 콜백 수신 → NumOpenPublicConnections 즉시 회복
// 따라서 단일 DestroySession 호출이 양쪽 시나리오 커버. 분기 코드 불필요(중복).
void USessionSubsystem::CleanupHostSession(const FString& Reason)
{
    const bool bWasHost = bIsLocalHost;
    const bool bHasNamedSession = SessionInterface.IsValid() &&
        SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;

    UE_LOG(LogSessionSubsystem, Error,
        TEXT("CleanupHostSession: role=%s hasNamedSession=%d reason=%s"),
        bWasHost ? TEXT("HOST (destroying lobby)")
                 : (bHasNamedSession ? TEXT("CLIENT (leaving host lobby)") : TEXT("none")),
        bHasNamedSession ? 1 : 0, *Reason);

    if (bHasNamedSession)
    {
        PendingCleanupReason = Reason;

        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
        DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionAfterCleanup));
        // 호스트: 로비 해체 / 클라: LeaveLobby (OSS 내부 분기)
        SessionInterface->DestroySession(NAME_GameSession);
        return;
    }

    // 정리할 세션이 없는 경우(CreateSession 자체 실패 직후 / 클라가 Join 전 거부 등) 즉시 broadcast
    bIsLocalHost = false;
    OnSessionErrorEvent.Broadcast(Reason);
}


void USessionSubsystem::OnDestroySessionAfterCleanup(FName /*SessionName*/, bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
    }

    // bIsLocalHost는 cleanup 진입 시점에 이미 결정됐고 콜백 도달 시점엔 의미 없음 (역할 식별용 마지막 로그)
    UE_LOG(LogSessionSubsystem, Warning,
        TEXT("OnDestroySessionAfterCleanup: ok=%d role=%s reason=%s"),
        bWasSuccessful ? 1 : 0,
        bIsLocalHost ? TEXT("HOST (lobby destroyed)") : TEXT("CLIENT (left host lobby)"),
        *PendingCleanupReason);

    bIsLocalHost = false;
    // 클라 측 stale state 청소 — 새 방 재시도 시 잔여 영향 차단
    PendingDediURL.Empty();

    const FString Reason = PendingCleanupReason;
    PendingCleanupReason.Empty();

    OnSessionErrorEvent.Broadcast(Reason);
}


void USessionSubsystem::FinalizeHostTravel(const FString& DediURL)
{
    // 1) SessionSettings에 데디 URL 심기 → 클라이언트가 검색 시 읽음
    if (SessionInterface.IsValid())
    {
        if (auto* Named = SessionInterface->GetNamedSession(NAME_GameSession))
        {
            Named->SessionSettings.Set(Key_DediEndpoint, DediURL,
                EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
            SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, true);
        }
    }

    // 2) 호스트 본인 데디로 이동
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
    {
        PC->ClientTravel(DediURL, ETravelType::TRAVEL_Absolute);
    }
}


// ---------------------------------------------------------
// 게임 시작/종료 시 세션 가시성 토글
// ---------------------------------------------------------
void USessionSubsystem::LockSessionForInGame()
{
    if (!bIsLocalHost || !SessionInterface.IsValid())
    {
        return;
    }

    FNamedOnlineSession* Named = SessionInterface->GetNamedSession(NAME_GameSession);
    if (Named == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("LockSessionForInGame: named session missing."));
        return;
    }

    // 가시성·합류경로는 유지 — 방 목록에 계속 보이도록.
    // 방 목록 위젯은 Key_GameStarted=1을 보고 클릭 비활성화한다.
    // 백스톱은 두 겹: (1) OSS — StartSession + bAllowJoinInProgress=false 조합으로 진행 중 합류 거부
    //                (2) 데디 MainGameMode::PreLogin — GamePhase != Lobby면 거부
    Named->SessionSettings.Set(Key_GameStarted, (int32)1, EOnlineDataAdvertisementType::ViaOnlineService);

    SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, /*bShouldRefreshOnlineData=*/true);

    // StartSession을 호출해야 bAllowJoinInProgress=false가 실제 발효 (InProgress 상태 진입)
    SessionInterface->StartSession(NAME_GameSession);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Session locked for in-game (GameStarted=1, StartSession fired)."));
}


void USessionSubsystem::UnlockSessionForLobby()
{
    if (!bIsLocalHost || !SessionInterface.IsValid())
    {
        return;
    }

    FNamedOnlineSession* Named = SessionInterface->GetNamedSession(NAME_GameSession);
    if (Named == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("UnlockSessionForLobby: named session missing."));
        return;
    }

    // 진행 상태 0 복귀 + 세션을 NotStarted로 되돌려 bAllowJoinInProgress 게이트 해제
    Named->SessionSettings.Set(Key_GameStarted, (int32)0, EOnlineDataAdvertisementType::ViaOnlineService);

    SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, true);
    SessionInterface->EndSession(NAME_GameSession);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Session unlocked for lobby (GameStarted=0, EndSession fired)."));
}


// ---------------------------------------------------------
// 2. 방 목록 검색
// ---------------------------------------------------------
void USessionSubsystem::FindRooms()
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("FindRooms: SessionInterface invalid — empty result."));
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }

    // 진행 중 검색이 있으면 중복 발사 차단 (Steam OSS는 동시 FindSessions를 깔끔히 처리 못함)
    if (SessionSearch.IsValid() && SessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("FindRooms: search already in progress — ignored."));
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = false;
    SessionSearch->MaxSearchResults = 10000;
    // SEARCH_LOBBIES — Steam OSS lobby 검색의 핵심 키. 이전 작동 코드 기준 복원.
    //                   (UE5.5+ deprecated 정보는 오인이었음 — Steam OSS는 여전히 이 키로 lobby 검색 트리거.)
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    // QuerySettings로 GameTag 필터링은 OSS 변환 버그로 0개 반환 → 클라측 SessionSettings.Get으로 필터링.

    // 이전 핸들 정리 후 재바인딩
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnFindSessionsComplete));

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("FindRooms: LocalPlayer null."));
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("FindRooms: 검색 시작."));
    SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
}


void USessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    }

    TArray<FRoomListEntry> Out;

    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("OnFindSessionsComplete: failed or invalid SessionSearch."));
        OnRoomsFoundEvent.Broadcast(false, Out);
        return;
    }

    const TArray<FOnlineSessionSearchResult>& Results = SessionSearch->SearchResults;

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Steam returned %d lobbies:"), Results.Num());
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& R = Results[i];
        const FOnlineSession& S = R.Session;

        // UE_LOG(LogSessionSubsystem, Warning,
        //     TEXT("  raw[%d]: Host=%s Max=%d Open=%d"),
        //     i, *S.OwningUserName, S.SessionSettings.NumPublicConnections,
        //     S.NumOpenPublicConnections);

        // Indian_Bab 로비만 통과
        // 키 없거나 값 mismatch면 외부 게임 로비로 간주하고 skip.
        int32 TagValue = 0;
        if (!S.SessionSettings.Get(Key_GameTag, TagValue) || TagValue != GameTagMagic)
        {
            continue;
        }
        
        FRoomListEntry Entry;
        Entry.HostName = S.OwningUserName;
        Entry.MaxPlayers = S.SessionSettings.NumPublicConnections;
        Entry.CurrentPlayers = FMath::Max(0, Entry.MaxPlayers - S.NumOpenPublicConnections);

        int32 GameStartedInt = 0;
        Entry.bGameStarted = S.SessionSettings.Get(Key_GameStarted, GameStartedInt) && GameStartedInt == 1;
        Entry.bFriendsOnly = S.SessionSettings.bAllowJoinViaPresenceFriendsOnly;
        Entry.SearchResultIndex = i;

        Out.Add(Entry);
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("OnFindSessionsComplete: %d rooms found."), Out.Num());
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        const FRoomListEntry& E = Out[i];
        UE_LOG(LogSessionSubsystem, Warning,
            TEXT("  room[%d]: Host=%s Players=%d/%d GameStarted=%s FriendsOnly=%s"),
            i, *E.HostName, E.CurrentPlayers, E.MaxPlayers,
            E.bGameStarted ? TEXT("YES") : TEXT("NO"),
            E.bFriendsOnly ? TEXT("YES") : TEXT("NO"));
    }
    OnRoomsFoundEvent.Broadcast(true, Out);
}


// ---------------------------------------------------------
// 3. 인덱스 기반 참가
// ---------------------------------------------------------
void USessionSubsystem::JoinRoomByIndex(int32 SearchResultIndex)
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: no SessionInterface/SessionSearch."));
        OnSessionErrorEvent.Broadcast(TEXT("세션 시스템이 준비되지 않았습니다. 방 검색을 다시 시도해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    if (!SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: index %d out of range (size=%d)."),
            SearchResultIndex, SessionSearch->SearchResults.Num());
        OnSessionErrorEvent.Broadcast(TEXT("선택한 방을 찾을 수 없습니다. 방 목록을 새로고침해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];

    // GameStarted 가드 (위젯 disable의 race 백스톱)
    int32 GameStartedInt = 0;
    if (Result.Session.SessionSettings.Get(Key_GameStarted, GameStartedInt) && GameStartedInt == 1)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: room already in-game — denied."));
        OnSessionErrorEvent.Broadcast(TEXT("게임이 이미 진행 중인 방입니다."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    // 데디 URL 추출 (호스트가 FinalizeHostTravel에서 박제한 "host_ip:port")
    PendingDediURL.Empty();
    Result.Session.SessionSettings.Get(Key_DediEndpoint, PendingDediURL);
    if (PendingDediURL.IsEmpty())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("JoinRoomByIndex: Key_DediEndpoint missing in target session."));
        OnSessionErrorEvent.Broadcast(TEXT("방 정보가 손상되었습니다. 다른 방을 시도해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    // 기존 세션 정리 (재참가/이전 참가 잔여)
    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
        bIsLocalHost = false;
    }

    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
    JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnJoinSessionComplete));

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("JoinRoomByIndex: LocalPlayer null."));
        OnSessionErrorEvent.Broadcast(TEXT("로컬 플레이어 정보를 가져올 수 없습니다."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: idx=%d dedi=%s — JoinSession 발사."),
        SearchResultIndex, *PendingDediURL);
    SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
}


void USessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
    }

    const bool bJoinOk = (Result == EOnJoinSessionCompleteResult::Success);

    if (bJoinOk)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("Joined Session Successfully!"));

        if (PendingDediURL.IsEmpty())
        {
            // Join은 성공해서 호스트 Lobby 멤버로 들어간 상태 — DediURL만 누락.
            // 그냥 return하면 호스트 슬롯 점유한 채 leak되므로 명시적 leave 필수.
            UE_LOG(LogSessionSubsystem, Error, TEXT("Join OK but Dedi endpoint missing in SessionSettings."));
            CleanupHostSession(TEXT("방 정보가 손상되었습니다. 다른 방을 시도해주세요."));
            OnJoinSessionCompleteEvent.Broadcast(false);
            return;
        }

        UE_LOG(LogSessionSubsystem, Warning, TEXT("Join OK → ClientTravel %s"), *PendingDediURL);
        if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
        {
            PC->ClientTravel(PendingDediURL, ETravelType::TRAVEL_Absolute);
        }
        OnJoinSessionCompleteEvent.Broadcast(true);
    }
    else
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Join Failed. Code: %d"), (int32)Result);
        // EOnJoinSessionCompleteResult를 사용자 친화 텍스트로 매핑
        FString Reason;
        switch (Result)
        {
        case EOnJoinSessionCompleteResult::SessionIsFull:           Reason = TEXT("방이 가득 찼습니다."); break;
        case EOnJoinSessionCompleteResult::SessionDoesNotExist:     Reason = TEXT("해당 방이 더 이상 존재하지 않습니다."); break;
        case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: Reason = TEXT("방 주소를 가져올 수 없습니다."); break;
        case EOnJoinSessionCompleteResult::AlreadyInSession:        Reason = TEXT("이미 다른 방에 입장한 상태입니다."); break;
        default:                                                     Reason = TEXT("방 입장에 실패했습니다."); break;
        }
        // Join 실패 시점에는 호스트 Lobby에 합류한 적이 없음 (OSS가 자체 정리).
        // 사유 모달만 띄우면 됨 — 명시적 DestroySession 불필요.
        OnSessionErrorEvent.Broadcast(Reason);
        OnJoinSessionCompleteEvent.Broadcast(false);
    }
}