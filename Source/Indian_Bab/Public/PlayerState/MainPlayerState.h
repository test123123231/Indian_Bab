// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MainPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTriggerCountChanged, int32);

UCLASS()
class INDIAN_BAB_API AMainPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMainPlayerState();

	// 상태 복제를 위한 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

public:
	// 플레이어의 생존 유무
	UPROPERTY(ReplicatedUsing = "OnRep_isAlive", BlueprintReadOnly, Category = "PlayerState")
	bool isAlive;

	// 플레이어의 폴드 유무
	UPROPERTY(ReplicatedUsing = "OnRep_isFold", BlueprintReadOnly, Category = "PlayerState")
	bool isFold;

	// 서브 리볼버의 탄환 위치
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PlayerState")
	TArray<bool> BulletArray;

	// 서브 리볼버 당김 횟수
	UPROPERTY(ReplicatedUsing = "OnRep_TotalTriggerCount", BlueprintReadOnly, Category = "PlayerState")
	int32 TotalTriggerCount;

	// 처음 서브 리볼버 설정
	void SetInitSubRevolver();

	// 리볼버 당김 횟수 변경
	bool ChangeSubRevolver();

	FOnTriggerCountChanged OnTriggerCountChanged;
protected:
	// 서브 리볼버의 당김 횟수가 바뀌었을 때 호출
	UFUNCTION()
	void OnRep_TotalTriggerCount();

	UFUNCTION()
	void OnRep_isAlive();

	UFUNCTION()
	void OnRep_isFold();
	
};
