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


const FName USessionSubsystem::Key_DediEndpoint = FName("DediEndpoint");


USessionSubsystem::USessionSubsystem()
{
}


void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 온라인 서브시스템(Steam) 가져오기
    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        SessionInterface = Subsystem->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("SessionSubsystem: Steam Subsystem Found!"));
        }
    }
}


void USessionSubsystem::Deinitialize()
{
    Super::Deinitialize();
    // 필요 시 정리 작업 수행
}


// ---------------------------------------------------------
// 방 생성
// ---------------------------------------------------------
void USessionSubsystem::CreateRoom(int32 MaxPlayers)
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
    }

    // 델리게이트 바인딩
    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnCreateSessionComplete));

    // 초대 코드 생성 (4자리 랜덤)
    CurrentInviteCode = GenerateRandomCode(4);

    // 세션 설정 구성
    FOnlineSessionSettings SessionSettings;

    // 스팀 설정 핵심
    SessionSettings.bIsLANMatch = false;        // 스팀은 LAN이 아님
    SessionSettings.bUsesPresence = true;       // Steam 등의 플레이어 존재 여부 기능 사용 (플레이어 상태 표시)
	SessionSettings.bAllowJoinInProgress = true; // 진행 중인 세션 입장 허용
	SessionSettings.bAllowJoinViaPresence = true; // 스팀 친구 목록 통해 입장할 수 있도록 허용
	SessionSettings.bShouldAdvertise = true;    // 세션 광고 허용
	SessionSettings.bUseLobbiesIfAvailable = true; // 스팀 로비 사용
    SessionSettings.NumPublicConnections = MaxPlayers; // 최대 인원

    // 초대 코드를 세션 데이터에 심기 (검색 시 필터링용)
    // ViaOnlineService : 스팀 서버를 통해 이 데이터를 전파함
    SessionSettings.Set(Key_InviteCode, CurrentInviteCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    // 4. 세션 생성 요청 (로컬 유저 ID 필요)
    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating Room with Code: %s"), *CurrentInviteCode);
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
        UE_LOG(LogTemp, Error, TEXT("Failed to Create Session."));
        OnCreateSessionCompleteEvent.Broadcast(false);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Steam Session OK. Requesting matchmaker..."));
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
        UE_LOG(LogTemp, Error, TEXT("Matchmaker request failed."));
        OnCreateSessionCompleteEvent.Broadcast(false);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    auto Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Matchmaker response parse failed: %s"), *JsonBody);
        OnCreateSessionCompleteEvent.Broadcast(false);
        return;
    }

    const FString DediURL = FString::Printf(TEXT("%s:%d"),
        *Json->GetStringField(TEXT("host_ip")),
        Json->GetIntegerField(TEXT("port")));

    UE_LOG(LogTemp, Warning, TEXT("Matchmaker response → Dedi: %s"), *DediURL);
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
// 2. 방 찾기 및 입장 (Join Room)
// ---------------------------------------------------------
void USessionSubsystem::JoinRoomByCode(FString InputCode)
{
    if (!SessionInterface.IsValid())
    {
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    // 입력된 코드 대문자 변환 및 저장
    TargetCodeToJoin = InputCode.ToUpper();

    // 검색 델리게이트 바인딩
    FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnFindSessionsComplete));

    // 검색 설정 (Search Settings)
    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    SessionSearch->bIsLanQuery = false;     // 스팀 사용 시 false
    SessionSearch->MaxSearchResults = 10000; // 최대한 많이 검색해서 코드를 찾아야 함

    // 스팀 로비(Presence) 검색 활성화
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

    // 검색 시작
    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("Searching for Room Code: %s ..."), *TargetCodeToJoin);
		SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
    }
}


void USessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    }

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("=== 검색 시작 ==="));
        UE_LOG(LogTemp, Warning, TEXT("총 검색된 세션 수: %d"), SessionSearch->SearchResults.Num());
        UE_LOG(LogTemp, Warning, TEXT("찾으려는 코드: %s"), *TargetCodeToJoin);

        bool bFound = false;

        // 검색된 모든 방을 순회하며 로그를 찍어봅니다.
        for (FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
        {
            FString ServerCode;
            FString HostName = SearchResult.Session.OwningUserName; // 방장 이름

            // 우리가 심어둔 초대 코드(Key_InviteCode)가 있는지 확인
            if (SearchResult.Session.SessionSettings.Get(Key_InviteCode, ServerCode))
            {
                UE_LOG(LogTemp, Log, TEXT("[방 발견] 방장: %s | 코드: %s"), *HostName, *ServerCode);

                if (ServerCode == TargetCodeToJoin)
                {
                    UE_LOG(LogTemp, Warning, TEXT("★★★ 코드 일치! 입장 시도... ★★★"));

                    // 호스트가 SessionSettings에 심어둔 데디 endpoint 추출
                    PendingDediURL.Empty();
                    SearchResult.Session.SessionSettings.Get(Key_DediEndpoint, PendingDediURL);
                    UE_LOG(LogTemp, Warning, TEXT("Found Dedi endpoint: %s"), *PendingDediURL);

                    JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
                        FOnJoinSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnJoinSessionComplete));

					SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable = true; // 스팀 로비 사용
					SearchResult.Session.SessionSettings.bUsesPresence = true; // 스팀 친구 목록 통해 입장할 수 있도록 허용

                    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
                    SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SearchResult);

                    bFound = true;
                    break; // 찾았으니 종료
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("[방 발견] 방장: %s | 초대 코드 없음 (다른 게임 방일 수 있음)"), *HostName);
            }
        }

        if (!bFound)
        {
            UE_LOG(LogTemp, Warning, TEXT("일치하는 코드를 가진 방을 찾지 못했습니다."));
            // UI에 실패 알림
            OnJoinSessionCompleteEvent.Broadcast(false);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Session Search Failed (Network Error or Timeout)."));
        OnJoinSessionCompleteEvent.Broadcast(false);
    }
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
        UE_LOG(LogTemp, Warning, TEXT("Joined Session Successfully!"));

        if (PendingDediURL.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("Join OK but Dedi endpoint missing in SessionSettings."));
            OnJoinSessionCompleteEvent.Broadcast(false);
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("Join OK → ClientTravel %s"), *PendingDediURL);
        if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
        {
            PC->ClientTravel(PendingDediURL, ETravelType::TRAVEL_Absolute);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Join Failed. Code: %d"), (int32)Result);
    }

    // 성공 여부 UI 전송
    OnJoinSessionCompleteEvent.Broadcast(bJoinOk && !PendingDediURL.IsEmpty());
}


// ---------------------------------------------------------
// 유틸리티
// ---------------------------------------------------------
FString USessionSubsystem::GenerateRandomCode(int32 Length)
{
    const FString Chars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    FString Result = TEXT("");
    for (int32 i = 0; i < Length; i++)
    {
        int32 Index = FMath::RandRange(0, Chars.Len() - 1);
        Result += Chars.Mid(Index, 1);
    }
    return Result;
}