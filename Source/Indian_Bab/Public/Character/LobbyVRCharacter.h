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
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void Tick(float DeltaTime) override;

	void BindVRPlayerStateDelegates();

	void UpdateNameWidget();

	void UpdateCardWidget();

	void InitSeatedAtSeat(ASeatActor* TargetSeat);

	UFUNCTION(Client, Reliable)
	void Client_InitSeatedAtSeat(FVector TargetLocation, FRotator TargetRotation);

	UFUNCTION(Client, Reliable)
	void Client_ShowReadyWidget();

	UFUNCTION(Client, Reliable)
	void Client_HideReadyWidget();

	void PressRightWidgetInteraction();
	void ReleaseRightWidgetInteraction();
	void PressLeftWidgetInteraction();
	void ReleaseLeftWidgetInteraction();

	void ShowReadyWidget();
	void HideReadyWidget();

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
	TObjectPtr<UMotionControllerComponent> MotionControllerLeftAim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteractionRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteractionLeft;

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
	FVector PlayerNameWidgetRelativeLocation = FVector(0.0f, 0.0f, 30.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI")
	FVector PlayerNameWidgetFallbackRelativeLocation = FVector(0.0f, 0.0f, 190.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|NameWidget")
	FVector VRNameWidgetWorldOffset = FVector(0.0f, 0.0f, 25.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|NameWidget")
	float VRNameWidgetHeightOffset = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|UI|Debug")
	bool bLogPlayerNameWidgetTransform = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	float VRPointerMaxDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	float LaserThickness = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	bool bShowVRPointer = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer|Debug")
	bool bShowWidgetInteractionDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	bool bLogVRPointerHits = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MotionController")
	FTransform Arm;

	UFUNCTION(Server, Unreliable)
	void Server_UpdateArm(const FTransform& NewArm);



protected:
	virtual void UpdateAimFromView() override;
	void UpdateArmPosition();
private:
	void ConfigureVRSeatedState();
	void ConfigureWidgetInteraction();
	void ConfigurePlayerNameWidget();
	void UpdateVRNameWidget();
	bool ApplyVRNameWidgetVisibility();
	void UpdateVRNameWidgetTransform();
	FVector GetVRNameWidgetTargetLocation() const;
	bool GetLocalCameraLocation(FVector& OutCameraLocation) const;
	void LogPlayerNameWidgetTransform() const;
	void HideLocalPlayerNameWidget();
	void UpdateVRPointers();
	void UpdateLaserPointer(const UMotionControllerComponent* AimController, const TCHAR* PointerName) const;
	void DrawSeatDebugCapsule() const;
};
