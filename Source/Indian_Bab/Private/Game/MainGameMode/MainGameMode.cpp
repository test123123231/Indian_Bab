#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "PlayerState/MainPlayerState.h"
#include "Character/LobbyVRCharacter.h"
#include "CardController/CardManager.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Network/NetworkEndpoints.h"



// 서버 전용 코드 — 클라 EXE(Type=Client, WITH_SERVER_CODE=0)에서는 통째로 컴파일되지 않음.
// 에디터/데디 빌드(WITH_SERVER_CODE=1)에서만 메서드 정의가 살아남는다.
// 외부 호출처(PlayerController, LobbyCharacter, SeatActor)도 동일 가드 필요.
#if WITH_SERVER_CODE

AMainGameMode::AMainGameMode()
{
	GameStateClass = AMainGameState::StaticClass();
}

void AMainGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// MM이 dedi spawn 시 주입한 -MatchId=<uuid>, -HostSteamId=<id>를 캐싱.
	// 없으면 빈 문자열(스탠드얼론 PIE 등).
	FParse::Value(FCommandLine::Get(), TEXT("MatchId="), CachedMatchId);
	FParse::Value(FCommandLine::Get(), TEXT("HostSteamId="), CachedHostSteamId);
	FParse::Value(FCommandLine::Get(), TEXT("MaxPlayers="), CachedMaxPlayers);
	UE_LOG(LogTemp, Warning, TEXT("[MainGameMode] InitGame CachedMatchId=%s CachedHostSteamId=%s CachedMaxPlayers=%d"),
		CachedMatchId.IsEmpty() ? TEXT("(none)") : *CachedMatchId,
		CachedHostSteamId.IsEmpty() ? TEXT("(none)") : *CachedHostSteamId,
		CachedMaxPlayers);
}

void AMainGameMode::NotifyACLeave(const FString& SteamId)
{
#if ANTICHEAT_USE_MOCK
	// AC 서버 미사용 — leave 통보 스킵
	return;
#else
	if (SteamId.IsEmpty()) return;

	const FString URL = NetworkEndpoints::AC::Internal::Leave();
	const FString Payload = FString::Printf(TEXT("{\"user_id\":\"%s\"}"), *SteamId);

	auto Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Payload);
	Req->ProcessRequest();
#endif
}

void AMainGameMode::NotifyMatchClose()
{
	if (CachedMatchId.IsEmpty()) return;

	const FString URL = NetworkEndpoints::MM::Internal::Close(CachedMatchId);

	auto Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->ProcessRequest();

	UE_LOG(LogTemp, Warning, TEXT("[MainGameMode] /close 발사 match=%s"), *CachedMatchId);
}

void AMainGameMode::NotifyMatchClearHost()
{
	if (CachedMatchId.IsEmpty()) return;

	const FString URL = NetworkEndpoints::MM::Internal::ClearHost(CachedMatchId);

	auto Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->ProcessRequest();

	UE_LOG(LogTemp, Warning, TEXT("[MainGameMode] /clear_host 발사 match=%s"), *CachedMatchId);
}

void AMainGameMode::Logout(AController* Exiting)
{
	// 떠나는 SteamID 추출 — Super 호출 전(PlayerState 정리되기 전).
	FString LeavingSteamId;
	if (Exiting && Exiting->PlayerState)
	{
		const FUniqueNetIdRepl& NetId = Exiting->PlayerState->GetUniqueId();
		if (NetId.IsValid())
		{
			LeavingSteamId = NetId->ToString();
		}
	}

	// 1) AC waiting 리셋 — AC가 SteamID로 verify_session 조회·리셋(단일 활성 토큰 정책).
	//    AC 미운영 시 HTTP fail-and-forget — 정상 입장(토큰 없는) 유저에게도 호출되지만
	//    AC 측 row 없으면 no-op.
	NotifyACLeave(LeavingSteamId);

	// 2) Super::Logout — 엔진이 NumPlayers를 1 감소시킴.
	Super::Logout(Exiting);

	// 3) MM 분기.
	if (NumPlayers <= 0)
	{
		// 마지막 플레이어 — 인스턴스 종료. MM이 데디 kill까지 처리.
		NotifyMatchClose();
	}
	else if (!CachedHostSteamId.IsEmpty() && LeavingSteamId == CachedHostSteamId)
	{
		// 호스트만 떠남, 남은 유저 있음 — host_steam_id NULL로 비우고 방 유지.
		NotifyMatchClearHost();
		CachedHostSteamId.Empty();  // 재발사 방지 (멱등은 백엔드도 보장하지만 짝 보강).
	}
}

void AMainGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	// 서버는 Null OSS, 클라이언트는 Steam ID → OSS 플랫폼 불일치 에러 무시
	if (ErrorMessage == TEXT("incompatible_unique_net_id"))
	{
		ErrorMessage = TEXT("");
	}

	// 백스톱: 게임이 이미 시작된 인스턴스는 신규 합류 거부 (방 목록 UI 비활성화를 뚫고 들어온 경우 차단)
	// ErrorMessage는 NMT_Failure 채널로 클라에 전달되어 GameInstance::OnNetworkFailure(ErrorString)로 노출됨
	if (const AMainGameState* GS = GetGameState<AMainGameState>())
	{
		if (GS->CurrentGamePhase != EGamePhase::Lobby)
		{
			ErrorMessage = TEXT("게임이 이미 진행 중인 방입니다.");
			UE_LOG(LogTemp, Warning, TEXT("[PreLogin] reject — game already in progress (phase=%d)"),
				(int32)GS->CurrentGamePhase);
			return;
		}
	}

	// 만석 체크 — MM이 dedi spawn 시 주입한 -MaxPlayers=N 캐시값 기준.
	// NumPlayers는 이미 PostLogin 된 인원이므로 신규 1명 합류 후 초과하면 거부.
	// CachedMaxPlayers==0(CLI 미주입, 스탠드얼론 PIE 등)이면 만석 체크 스킵.
	if (CachedMaxPlayers > 0 && NumPlayers >= CachedMaxPlayers)
	{
		ErrorMessage = FString::Printf(TEXT("방이 가득 찼습니다. (%d/%d)"),
			NumPlayers, CachedMaxPlayers);
		UE_LOG(LogTemp, Warning, TEXT("[PreLogin] reject — server full (%d/%d)"),
			NumPlayers, CachedMaxPlayers);
		return;
	}
}

void AMainGameMode::PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete)
{
	// 1) 동기 합류 조건 — 기존 PreLogin 재사용 (phase·만석·incompatible_net_id).
	FString SyncError;
	PreLogin(Options, Address, UniqueId, SyncError);
	if (!SyncError.IsEmpty())
	{
		OnComplete.ExecuteIfBound(SyncError);
		return;
	}

#if ANTICHEAT_USE_MOCK
	// AC 서버 미사용 — 동기 조건만 통과하면 즉시 허용
	UE_LOG(LogTemp, Log, TEXT("[PreLoginAsync] ANTICHEAT_USE_MOCK — AC 검증 스킵"));
	OnComplete.ExecuteIfBound(FString());
	return;
#else
	// 2) UniqueId(SteamID)로 AC prelogin 비동기 검증.
	//    전 시스템이 SteamID 키로 통일 — user_id PK로 row 1:1 매칭.
	const FString SteamId = UniqueId.IsValid() ? UniqueId->ToString() : FString();
	if (SteamId.IsEmpty())
	{
		OnComplete.ExecuteIfBound(TEXT("Steam ID를 확인할 수 없습니다."));
		return;
	}

	const FString URL = NetworkEndpoints::AC::Internal::PreLogin();
	const FString Payload = FString::Printf(
		TEXT("{\"user_id\":\"%s\",\"match_id\":\"%s\"}"),
		*SteamId, *CachedMatchId);

	auto Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Payload);
	Req->SetTimeout(10.0f);

	FOnPreLoginCompleteDelegate CompleteCopy = OnComplete;
	Req->OnProcessRequestComplete().BindLambda(
		[SteamId, CompleteCopy](FHttpRequestPtr, FHttpResponsePtr Resp, bool bHttpOk)
		{
			if (!bHttpOk || !Resp.IsValid() || Resp->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PreLoginAsync] prelogin http fail (ok=%d code=%d)"),
					bHttpOk ? 1 : 0, Resp.IsValid() ? Resp->GetResponseCode() : 0);
				CompleteCopy.ExecuteIfBound(TEXT("안티치트 서버 연결 실패."));
				return;
			}

			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
			bool bValid = false;
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()
				|| !Json->TryGetBoolField(TEXT("valid"), bValid) || !bValid)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PreLoginAsync] reject steam_id=%s — not verified"), *SteamId);
				CompleteCopy.ExecuteIfBound(TEXT("안티치트 검증 실패."));
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("[PreLoginAsync] OK steam_id=%s"), *SteamId);
			CompleteCopy.ExecuteIfBound(FString());
		});
	Req->ProcessRequest();
#endif // ANTICHEAT_USE_MOCK
}

void AMainGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		FTimerDelegate SeatDelegate;
		SeatDelegate.BindUObject(this, &AMainGameMode::AssignInitialSeatToPlayer, NewPlayer);
		GetWorldTimerManager().SetTimerForNextTick(SeatDelegate);
	}

	// 접속한 플레이어 수 로그 (NumPlayers는 AGameMode 기본 변수)
	UE_LOG(LogTemp, Warning, TEXT("플레이어 접속 완료. 현재 인원: %d"), NumPlayers);
}

void AMainGameMode::HandlePlayerReady(APlayerController* ReadyPlayer)
{
	if (!HasAuthority() || !ReadyPlayer)
	{
		return;
	}

	if (bGameStartRequested)
	{
		return;
	}

	if (bAutoReadyAllPlayersWhenOneReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Test auto ready enabled. Adding all connected players."));

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC)
			{
				continue;
			}

			const bool bWasAlreadyReady = ReadyPlayers.Contains(PC);
			ReadyPlayers.AddUnique(PC);

			if (!bWasAlreadyReady)
			{
				UE_LOG(LogTemp, Warning, TEXT("[GM] Ready player added: %s"), *GetNameSafe(PC));
			}
		}
	}
	else
	{
		const bool bWasAlreadyReady = ReadyPlayers.Contains(ReadyPlayer);
		ReadyPlayers.AddUnique(ReadyPlayer);

		if (!bWasAlreadyReady)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Ready player added: %s"), *GetNameSafe(ReadyPlayer));
		}
	}

	if (!IsCurrentPlayerCountInGameRange())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Ready ignored. Connected players = %d, required range = %d-%d"),
			NumPlayers, MinPlayerCountToStart, MaxPlayerCountToStart);
		return;
	}

	if (AMainGameState* GS = GetGameState<AMainGameState>())
	{
		GS->ChangeReadyPlayerCount(ReadyPlayers.Num());
	}

	const int32 Required = GetRequiredReadyPlayerCount();
	UE_LOG(LogTemp, Warning, TEXT("[GM] Ready count = %d / Required = %d"), ReadyPlayers.Num(), Required);

	if (ReadyPlayers.Num() >= Required)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] All players ready. Start game."));
		StartGameAfterAllReady();
	}
}

int32 AMainGameMode::GetRequiredReadyPlayerCount() const
{
	return NumPlayers;
}

bool AMainGameMode::IsCurrentPlayerCountInGameRange() const
{
	return NumPlayers >= MinPlayerCountToStart && NumPlayers <= MaxPlayerCountToStart;
}

void AMainGameMode::AssignInitialSeatToPlayer(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS)
	{
		return;
	}

	ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(NewPlayer->GetPawn());
	if (!VRCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Initial auto seating skipped. Pawn is not LobbyVRCharacter."));
		return;
	}

	ASeatActor* EmptySeat = FindEmptySeat();
	if (!EmptySeat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Initial auto seating failed. Empty seat not found."));
		return;
	}

	if (!EmptySeat->SitTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Initial auto seating failed. Seat has no SitTarget."));
		return;
	}

	EmptySeat->SetOccupant(VRCharacter);

	if (!GS->SeatChairArray.Contains(EmptySeat))
	{
		GS->SeatChairArray.Add(EmptySeat);
		GS->SeatChairArray.Sort([](const ASeatActor& A, const ASeatActor& B)
		{
			return A.SeatOrder < B.SeatOrder;
		});
	}

	VRCharacter->InitSeatedAtSeat(EmptySeat);

	UE_LOG(LogTemp, Warning, TEXT("[GM] LobbyVRCharacter was initially seated at SeatOrder %d."), EmptySeat->SeatOrder);
}

ASeatActor* AMainGameMode::FindEmptySeat()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS)
	{
		return nullptr;
	}

	TArray<AActor*> FoundActors;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		ASeatActor::StaticClass(),
		FoundActors
	);

	TArray<ASeatActor*> Seats;

	for (AActor* Actor : FoundActors)
	{
		ASeatActor* Seat = Cast<ASeatActor>(Actor);
		if (!Seat)
		{
			continue;
		}

		Seats.Add(Seat);
	}

	Seats.Sort([](const ASeatActor& A, const ASeatActor& B)
	{
		return A.SeatOrder < B.SeatOrder;
	});

	for (ASeatActor* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}

		if (!GS->SeatChairArray.Contains(Seat) && !Seat->GetOccupant())
		{
			return Seat;
		}
	}

	return nullptr;
}

void AMainGameMode::StartGameAfterAllReady()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bGameStartRequested)
	{
		return;
	}

	bGameStartRequested = true;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] All players ready. Game starts in 3 seconds."));

	GS->SetGamePhase(EGamePhase::Starting);

	// 각 플레이어 서브 리볼버 초기화
	for (APlayerState* PS : GS->PlayerArray)
	{
		AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
		if (MPS)
		{
			MPS->SetInitSubRevolver();
		}
	}

	// 기준 플레이어 초기화
	CheckPlayer = -1;

	// 카드 매니저 초기화
	MainCardManager = GetCardManager();
	if (!MainCardManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] No CardManager"));
		return;
	}

	MainCardManager->InitializeDeck();

	// 3초 뒤 게임 시작
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::StartMainGame, 3.0f,false);
}

void AMainGameMode::PlayerSeated(APlayerController* SeatedPlayer, ASeatActor* SeatedChair)
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS && SeatedPlayer && SeatedChair)
	{
		// 중복 방지
		if(GS -> SeatChairArray.Contains(SeatedChair)) return;

		GS -> SeatChairArray.Add(SeatedChair);
	
		// SeatOrder 기준 정렬
        GS->SeatChairArray.Sort([](const ASeatActor& A, const ASeatActor& B)
        {
            return A.SeatOrder < B.SeatOrder;
        });

		int32 NewPlayerCount = GS -> SeatChairArray.Num();
		GS -> ChangeReadyPlayerCount(NewPlayerCount);

		CheckGameStart();
	}
}

// 게임 시작 조건이 충족되었을 때 실행 및 초기화
void AMainGameMode::CheckGameStart()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;
	if (bGameStartRequested) return;
	if (!IsCurrentPlayerCountInGameRange()) return;

	// 기획 상 3~4인 플레이. 테스트를 위해 1인 이상으로 할 수도 있음.
	// 여기서는 현재 접속한 인원이 모두 앉았는지(Ready) 검사
	const int32 Required = GetRequiredReadyPlayerCount();
	if (GS->ReadyPlayerCount >= Required && GS->ReadyPlayerCount == NumPlayers)
	{
		UE_LOG(LogTemp, Warning, TEXT("모든 플레이어가 착석했습니다. 3초 후 게임을 시작합니다."));
		bGameStartRequested = true;

		GS->SetGamePhase(EGamePhase::Starting);

		// 각 플레이어 서브 리볼버 초기화(제일 처음에만/ 매 라운드x)
		for(APlayerState* PS : GS -> PlayerArray)
		{
			AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
			if(MPS)
			{
				MPS->SetInitSubRevolver();
			}
		}

		// 기준 플레이어 초기화
		CheckPlayer = -1;

		//  카드 매니저 초기화
		MainCardManager = GetCardManager();
		if(!MainCardManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("No CardManager"));
			return;
		}
		MainCardManager -> InitializeDeck();

		// 3초 뒤에 StartMainGame 함수 실행
		GetWorldTimerManager().ClearTimer(TimerHandle);
		GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::StartMainGame, 3.0f, false);
	}
}

// MainGame 시작(반복됨)
void AMainGameMode::StartMainGame()
{
	if (bGameEnded) return;

	// TODO: 카드 분배, 앤티(Ante) 지불 등 실제 인게임 로직 호출, 생존자 카운팅
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;
	
	GS->SetGamePhase(EGamePhase::Playing);

	//GS의 게임 페이즈 기반 플레이어 선택
	PickPlayer(GS -> CurrentPlayerIndex);
	
	// 라운드 내 isFold 초기화
	ResetFoldState();
	
	DistributeCard();
	
	StartTurnTimer(5.0f);

	return;
}

// 턴 넘기는 타이머
void AMainGameMode::StartTurnTimer(float Time)
{
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::OnTurnTimerExpired, Time, false);
	return;
}

// 턴 제한시간은 넘겼을 때
void AMainGameMode::OnTurnTimerExpired()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	UE_LOG(LogTemp, Warning, TEXT("[GM] TimeOut NextTurn"));
	GS -> ChangeCurrentBetInfo(EBetAction::CheckCall);
	CheckNext();
	return;
}

// 다음 행동(NextTurn, NextRound, ReStart) 체크 함수
void AMainGameMode::CheckNext()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	// 생존 인원이 1명일 때
	if(GS -> AlivePlayerCount <= 1)
	{
		EndGame(GetLastAlivePlayer());
		return;
	}
	GS->bTurnActionInProgress = false;

	// 활성 인원(비폴드 + 생존)이 1명 이하면 이번 라운드 종료
	int32 ActivePlayer = UpdateActivePlayer(GS);
	if(ActivePlayer <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Betting round ended: only one active player left"));
		NextRound();
		return;
	}

	// 다음 플레이어 PS 획득
	AMainPlayerState* NextPS = GetNextPlayerState(GS->CurrentPlayerIndex);
	if(!NextPS) return;
	if (bCheckPlayerFolded)
	{
		CheckPlayer = NextPS->GetPlayerId();
		bCheckPlayerFolded = false;
		NextTurn(NextPS);
		return;
	}

	// CheckPlayer와 다음 플레이어가 동일 할 때 
	if(CheckPlayer == NextPS -> GetPlayerId())
	{
		CheckPlayerCard();
		return;
	}
	//CheckPlayer와 다음 플레이어가 다를 때
	else
	{
		NextTurn(NextPS);
		return;
	}
}

//격발 페이즈 관리
void AMainGameMode::ManageShotPhase()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    if (GS->CurrentGamePhase != EGamePhase::Result || !CurrentWinnerPS)
    {
		FinishMainShotPhase();
        return;
    }

    if (GS -> CurrentBulletCount <= 0)
    {
        StartMainRevolverPutBack();
        return;
    }
	else
	{
		StartMainshotTimer(10.0f);
	}
}

// 격발 페이즈 종료 후 정리
void AMainGameMode::FinishMainShotPhase()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(TimerHandle);

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    CurrentWinnerPS = nullptr;
	GS -> CurrentBulletCount = 0;

	if (GS->AlivePlayerCount <= 1)
	{
		EndGame(GetLastAlivePlayer());
		return;
	}

    NextRound();
}

// 다음 턴으로
void AMainGameMode::NextTurn(AMainPlayerState* NextPS)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	GS->ChangeGameTurn(NextPS->GetPlayerId(), GS -> CurrentPlayerIndex);
	StartTurnTimer(5.0f);
	return;
}

// 다음 라운드로 넘기는 함수
void AMainGameMode::NextRound()
{
	if (!HasAuthority()) return;
	if (bGameEnded) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;
	
	//타이머 정리
	GetWorldTimerManager().ClearTimer(TimerHandle);

	// 다음 라운드 대비 GateState 초기화
	GS -> SetNextRoundGameState();

	// 메인 게임 새로 시작
	StartMainGame();
}

// 라운드 내 isFold 초기화
void AMainGameMode::ResetFoldState()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	for (APlayerState* PS : GS->PlayerArray)
	{
		AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
		if (!MPS) continue;

		MPS->isFold = false;
	}
}

AMainPlayerState* AMainGameMode::GetLastAlivePlayer()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return nullptr;

	for (APlayerState* PS : GS->PlayerArray)
	{
		AMainPlayerState* MPS = Cast<AMainPlayerState>(PS);
		if (MPS && MPS->isAlive)
		{
			return MPS;
		}
	}

	return nullptr;
}

void AMainGameMode::EndGame(AMainPlayerState* WinnerPS)
{
	if (!HasAuthority() || bGameEnded) return;

	bGameEnded = true;
	GetWorldTimerManager().ClearTimer(TimerHandle);

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS)
	{
		GS->SetGamePhase(EGamePhase::Result);
		GS->bTurnActionInProgress = true;
	}

	FString WinnerName = TEXT("Unknown Player");
	if (WinnerPS)
	{
		WinnerName = WinnerPS->GetSteamNickname();
		if (WinnerName.IsEmpty())
		{
			WinnerName = WinnerPS->GetPlayerName();
		}
	}

	const int32 WinnerPlayerId = WinnerPS ? WinnerPS->GetPlayerId() : -1;

	UE_LOG(LogTemp, Warning, TEXT("[GM] Game ended. Winner=%s PlayerId=%d"), *WinnerName, WinnerPlayerId);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(PC->GetPawn());
		if (VRCharacter)
		{
			VRCharacter->Client_ShowResultWidget(WinnerName, WinnerPlayerId);
		}
	}
}

#endif // WITH_SERVER_CODE
