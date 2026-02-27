// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameMode/MainGameTypes.h"
#include "MainGameMode.generated.h"

UCLASS()
class INDIAN_BAB_API AMainGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
    AMainGameMode();

	virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="State")
    EMainGamePhase Phase = EMainGamePhase::WaitingPlayers;

};
