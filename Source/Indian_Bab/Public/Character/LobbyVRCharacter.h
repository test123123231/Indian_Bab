#pragma once

#include "CoreMinimal.h"
#include "Character/LobbyCharacter.h"
#include "LobbyVRCharacter.generated.h"

class ASeatActor;
class UMotionControllerComponent;
class UReadyWidget;
class USceneComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;

UCLASS()
class INDIAN_BAB_API ALobbyVRCharacter : public ALobbyCharacter
{
	GENERATED_BODY()

public:
	ALobbyVRCharacter();

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void Tick(float DeltaTime) override;

	void InitSeatedAtSeat(ASeatActor* TargetSeat);

	UFUNCTION(Client, Reliable)
	void Client_InitSeatedAtSeat(FVector TargetLocation, FRotator TargetRotation);

	UFUNCTION(Client, Reliable)
	void Client_ShowReadyWidget();

	UFUNCTION(Client, Reliable)
	void Client_HideReadyWidget();

	virtual void OnRep_IsSitting() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerRightGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerLeftGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerRightAim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteractionRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR UI")
	TObjectPtr<UWidgetComponent> ReadyWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI")
	TSubclassOf<UReadyWidget> ReadyWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat")
	bool bUseCapsuleHalfHeightSeatOffset = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat")
	float SeatHeightOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat|Debug")
	bool bShowSeatDebugCapsule = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat|Debug")
	float DebugSeatCapsuleDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI")
	bool bAttachPlayerNameWidgetToHeadSocket = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI")
	FName PlayerNameWidgetHeadSocketName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI")
	float PlayerNameWidgetHeadSocketHeightOffset = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI")
	float PlayerNameWidgetHeightOffset = 180.0f;

protected:
	virtual void UpdateAimYawFromView() override;

private:
	void ConfigureVRSeatedState();
	void ConfigurePlayerNameWidget();
	void HideLocalPlayerNameWidget();
	void DrawSeatDebugCapsule() const;
	void ShowReadyWidget();
	void HideReadyWidget();
};
