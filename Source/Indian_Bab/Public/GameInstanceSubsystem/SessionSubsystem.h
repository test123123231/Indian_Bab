// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "SessionSubsystem.generated.h"


// UI에서 결과를 알 수 있도록 블루프린트 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreateSessionResult, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJoinSessionResult, bool, bWasSuccessful);


/**
 * 스팀 세션 관리용 서브시스템
 * GameInstance의 수명과 동일하게 자동 관리됩니다.
 */
UCLASS()
class INDIAN_BAB_API USessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    USessionSubsystem();

    // 서브시스템 초기화 및 종료 (생성자/소멸자 역할)
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ----------------------------------------------------------------
    // 블루프린트에서 호출할 함수들 (UI 버튼과 연결)
    // ----------------------------------------------------------------

    // 방 생성 (초대 코드 자동 생성)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void CreateRoom(int32 MaxPlayers);

    // 방 찾기 및 입장 (초대 코드 입력)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinRoomByCode(FString InputCode);

    // 현재 방의 초대 코드를 가져오는 함수 (로비 UI 표시용)
    UFUNCTION(BlueprintPure, Category = "Session")
    FString GetCurrentInviteCode() const { return CurrentInviteCode; }

    // ----------------------------------------------------------------
    // 델리게이트 (UI 이벤트 바인딩용)
    // ----------------------------------------------------------------
    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnCreateSessionResult OnCreateSessionCompleteEvent;

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnJoinSessionResult OnJoinSessionCompleteEvent;

protected:
    // 내부적으로 사용할 변수들
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<class FOnlineSessionSearch> SessionSearch;

    // 생성된 방의 초대 코드
    FString CurrentInviteCode;
    // 입장하려는 목표 초대 코드
    FString TargetCodeToJoin;

    // 세션 설정 키 (초대 코드를 저장할 키 이름)
    const FName Key_InviteCode = FName("RoomCode");

    // ----------------------------------------------------------------
    // 내부 콜백 함수 (OnlineSubsystem으로부터 응답을 받음)
    // ----------------------------------------------------------------
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    // 헬퍼 함수
    FString GenerateRandomCode(int32 Length);

    // 델리게이트 핸들 (바인딩 해제용)
    FDelegateHandle CreateSessionCompleteDelegateHandle;
    FDelegateHandle FindSessionsCompleteDelegateHandle;
    FDelegateHandle JoinSessionCompleteDelegateHandle;
	
};
