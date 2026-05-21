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


const FName USessionSubsystem::Key_DediEndpoint = FName("DediEndpoint");
const FName USessionSubsystem::Key_GameStarted = FName("GameStarted");


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
        OnCreateSessionCompleteEvent.Broadcast(false);
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
    // 게임 진행 중 신규 합류 차단 — 추가 런타임 락은 LockSessionForInGame()
    SessionSettings.bAllowJoinInProgress = false;
    SessionSettings.bAllowInvites = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.NumPublicConnections = MaxPlayers;

    // 가시성 분기 — Public: 누구나 검색/합류 / FriendsOnly: Steam 친구만 (k_ELobbyTypeFriendsOnly 매핑)
    const bool bPublic = (Visibility == ERoomVisibility::Public);
    SessionSettings.bAllowJoinViaPresence = bPublic;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = !bPublic;

    // 방 목록 위젯이 읽을 진행 상태 — 초기 0(로비), LockSessionForInGame에서 1로 갱신
    SessionSettings.Set(Key_GameStarted, (int32)0, EOnlineDataAdvertisementType::ViaOnlineService);

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
        OnCreateSessionCompleteEvent.Broadcast(false);
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
    // ── [매치메이커 서버 도입 시 활성화] 실제 HTTP 호출 ─────────────────
    // auto Req = FHttpModule::Get().CreateRequest();
    // Req->SetURL(MatchmakerConfig::BaseURL + MatchmakerConfig::CreateMatchPath);
    // Req->SetVerb(TEXT("POST"));
    // Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    // Req->SetContentAsString(TEXT("{}"));  // 추후 호스트 SteamID 등 주입
    // Req->OnProcessRequestComplete().BindLambda(
    //     [this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
    //     {
    //         HandleMatchmakerResponse(
    //             Resp.IsValid() ? Resp->GetContentAsString() : FString(),
    //             bOk && Resp.IsValid() && Resp->GetResponseCode() == 200);
    //     });
    // Req->ProcessRequest();
    // return;

    // ── [현재: 매치메이커 응답 모킹] ─────────────────────────────────
    // 매치메이커 서버가 준비되면 위 블록 활성화 + 아래 두 줄 제거.
    const FString MockResponse = TEXT(R"({"match_id":"TEST_MATCH_001","host_ip":"127.0.0.1","port":7777})");
    HandleMatchmakerResponse(MockResponse, true);
}


void USessionSubsystem::HandleMatchmakerResponse(const FString& JsonBody, bool bOk)
{
    if (!bOk)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker request failed."));
        OnCreateSessionCompleteEvent.Broadcast(false);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    auto Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker response parse failed: %s"), *JsonBody);
        OnCreateSessionCompleteEvent.Broadcast(false);
        return;
    }

    const FString DediURL = FString::Printf(TEXT("%s:%d"),
        *Json->GetStringField(TEXT("host_ip")),
        Json->GetIntegerField(TEXT("port")));

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Matchmaker response → Dedi: %s"), *DediURL);
    FinalizeHostTravel(DediURL);
}


void USessionSubsystem::FinalizeHostTravel(const FString& DediURL)
{
    // 1) SessionSettings에 데디 URL 심기 → 클라이언트가 검색 시 읽음
    if (SessionInterface.IsValid())
    {
        if (auto* Named = SessionInterface->GetNamedSession(NAME_GameSession))
        {
            Named->SessionSettings.Set(Key_DediEndpoint, DediURL,
                EOnlineDataAdvertisementType::ViaOnlineService);
            SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, true);
        }
    }

    // 2) 위젯에 결과 알림
    OnCreateSessionCompleteEvent.Broadcast(true);

    // 3) 호스트 본인 데디로 이동
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
// 2. 방 입장 — 방 목록 위젯 도입 전까지 stub (즉시 실패 브로드캐스트)
// ---------------------------------------------------------
void USessionSubsystem::JoinRoomByCode(FString InputCode)
{
    UE_LOG(LogSessionSubsystem, Warning,
        TEXT("JoinRoomByCode is deprecated — 초대코드 모델이 제거되었습니다. 방 목록 위젯이 도입되면 교체 예정."));
    OnJoinSessionCompleteEvent.Broadcast(false);
}


void USessionSubsystem::OnFindSessionsComplete(bool /*bWasSuccessful*/)
{
    // 후속 작업(방 목록 위젯) 도입 시 구현 — 현재는 호출 경로 없음
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
            UE_LOG(LogSessionSubsystem, Error, TEXT("Join OK but Dedi endpoint missing in SessionSettings."));
            OnJoinSessionCompleteEvent.Broadcast(false);
            return;
        }

        UE_LOG(LogSessionSubsystem, Warning, TEXT("Join OK → ClientTravel %s"), *PendingDediURL);
        if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
        {
            PC->ClientTravel(PendingDediURL, ETravelType::TRAVEL_Absolute);
        }
    }
    else
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Join Failed. Code: %d"), (int32)Result);
    }

    // 성공 여부 UI 전송
    OnJoinSessionCompleteEvent.Broadcast(bJoinOk && !PendingDediURL.IsEmpty());
}