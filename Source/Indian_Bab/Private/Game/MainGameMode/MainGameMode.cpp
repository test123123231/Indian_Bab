#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "PlayerState/MainPlayerState.h"
#include "Character/LobbyVRCharacter.h"
#include "CardController/CardManager.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// 서버 전용 코드 — 클라 EXE(Type=Client, WITH_SERVER_CODE=0)에서는 통째로 컴파일되지 않음.
// 에디터/데디 빌드(WITH_SERVER_CODE=1)에서만 메서드 정의가 살아남는다.
// 외부 호출처(PlayerController, LobbyCharacter, SeatActor)도 동일 가드 필요.
#if WITH_SERVER_CODE

AMainGameMode::AMainGameMode()
{
	GameStateClass = AMainGameState::StaticClass();
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

	// 만석 체크 — GameSession->MaxPlayers (Engine.ini의 [/Script/Engine.GameSession] MaxPlayers) 기준.
	// NumPlayers는 이미 PostLogin 된 인원이므로 신규 1명 합류 후 초과하면 거부.
	if (GameSession && GameSession->MaxPlayers > 0 && NumPlayers >= GameSession->MaxPlayers)
	{
		ErrorMessage = FString::Printf(TEXT("방이 가득 찼습니다. (%d/%d)"),
			NumPlayers, GameSession->MaxPlayers);
		UE_LOG(LogTemp, Warning, TEXT("[PreLogin] reject — server full (%d/%d)"),
			NumPlayers, GameSession->MaxPlayers);
		return;
	}
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
	// TODO: 카드 분배, 앤티(Ante) 지불 등 실제 인게임 로직 호출, 생존자 카운팅
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;
	
	GS->SetGamePhase(EGamePhase::Playing);

	//GS의 게임 페이즈 기반 플레이어 선택
	PickPlayer(GS -> CurrentPlayerIndex);
	
	// 라운드 내 isFold 초기화
	ResetFoldState();
	
	DistributeCard();
	
	StartTurnTimer(10.0f);

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
	if(GS -> AlivePlayerCount == 1)
	{
		// 게임 종료로 넘어감
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

    NextRound();
}

// 다음 턴으로
void AMainGameMode::NextTurn(AMainPlayerState* NextPS)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	GS->ChangeGameTurn(NextPS->GetPlayerId(), GS -> CurrentPlayerIndex);
	StartTurnTimer(20.0f);
	return;
}

// 다음 라운드로 넘기는 함수
void AMainGameMode::NextRound()
{
	if (!HasAuthority()) return;

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

#endif // WITH_SERVER_CODE
