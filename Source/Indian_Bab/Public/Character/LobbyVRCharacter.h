#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/MainGameTypes.h"
#include "LobbyVRCharacter.generated.h"

class UInputAction;
class AMainGamePlayerController;
struct FInputActionValue;
class UCameraComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class ASeatActor;
class ARevolver;
class UWidgetComponent;
class UReadyWidget;

UCLASS()
class INDIAN_BAB_API ALobbyVRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALobbyVRCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ������ ��� Ŭ���̾�Ʈ���� �ִϸ��̼��� ����϶�� �����ϴ� �Լ�
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySitAnimation();

	// ���� ��ȣ�ۿ� �� ȣ��Ǿ� ��Ÿ�� ���Ḧ ��ٸ��ϴ�.
	void StartSitTransition(ASeatActor* TargetSeat);

	// ����/Ŭ���̾�Ʈ ��ο��� �ɱ� ���°� ���� �� �ð���, ������ ó���� �� �Լ�
	UFUNCTION()
	void OnRep_IsSitting();

	void SetSittingState(bool bSitting);

	UFUNCTION(Client, Unreliable)
	void Client_PrepareSit(FVector TargetLocation, FRotator TargetRotation);

	// ������ ��Ÿ�� ���� �� Ŭ���̾�Ʈ���� ���� ī�޶� ������ �����ϴ� �Լ�
	UFUNCTION(Client, Reliable)
	void Client_LockCameraAfterSit(FRotator FinalSitRotation);

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> SitMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> AimMyselfMontage;

	// Fold �ݹ� ������ �����δ� �ִϸ��̼� ��Ÿ�� (BP���� �Ҵ�)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> EndAimMyselfMontage;

	// �¸� �� ���� �ܳ��ϴ� �ִϸ��̼� ��Ÿ�� (BP���� �Ҵ�)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> WinAimMontage;

	// �¸� �� ���� �ݹ� �� �ǵ����� �ִϸ��̼� ��Ÿ�� (BP���� �Ҵ�)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> WinEndMontage;

	// ���� ����� ���� - ABP ������Ʈ �ӽ� Ʈ������ �Ǻ��� (Replicated)
	UPROPERTY(ReplicatedUsing = OnRep_GunHoldReason, BlueprintReadOnly, Category = "State")
	EGunHoldReason GunHoldReason;

	// ABP�� �Ѱ��� ���� �¿� �� ���� ���� (��� ������� ����ȭ��)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera")
	float ReplicatedAimYaw = 0.0f;

	// �������� ���� �г��� �� ī�� ���ε�
	virtual void PossessedBy(AController* NewController) override;

	// ���� �г��� �� ī�� ���ε� �Լ�
	void BindPlayerStateDelegates();

	UFUNCTION()
	void UpdateNameWidget();

	UFUNCTION()
	void UpdateCardWidget();

	UFUNCTION(Server, Unreliable)
	void Server_UpdateAimYaw(float NewYaw);

	UFUNCTION()
	void OnRep_GunHoldReason();

	// �� ������ ��Ÿ�� ��� (Fold/Win ����)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayGrabGunMontage(EGunHoldReason Reason);

	// �� ���� ��ġ�� ������ ��Ÿ�� ��� (Fold/Win ����)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PutBackGunMontage(EGunHoldReason Reason);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> VRHead;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> VRBody;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ARevolver> DeskRevolver;

	// ���� �ִϸ��̼ǿ��� �տ� ���� ������
	// Fold�� ���� �ڸ� �� ���� ������, Win�� ���� �� �߾� ���� ������
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ARevolver> ActiveRevolver;

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	// UCameraComponent* CameraComponent;

	// AnimNotify_GrabRevolver ���� ȣ�� - å�� �������� ����� FP/TP �޽ø� ���Ͽ� ����
	void AttachRevolverToSocket();

	// AnimNotify_ReturnRevolverToDesk���� ȣ�� - �� ������ �޽ø� ����� å�� �������� �ٽ� ���̰� ��
	void ReturnRevolverToDesk();

	// �� �߰�: ���� ĳ���Ͱ� �ɾ��ִ��� ���� (����ȭ ��)
	UPROPERTY(ReplicatedUsing = OnRep_IsSitting, BlueprintReadOnly, Category = "State")
	bool bIsSitting;

	// �ɱ� ��Ÿ�ְ� �������� ���� (���������� ����)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsSittingEnded;

	// ��ȣ�ۿ� �Ÿ�
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerNameWidget")
	TObjectPtr<UWidgetComponent> PlayerNameWidgetComponent;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsPuttingBackGun = false;

	void SetActiveRevolver(ARevolver* NewRevolver);

	// ���� ������ ���ؼ� ǥ�� ����
	UPROPERTY(BlueprintReadOnly, Category = "Main Revolver")
	bool bShowMainShotAimLine = false;

	// ���ؼ� �Ÿ�
	UPROPERTY(EditDefaultsOnly, Category = "Main Revolver")
	float MainShotAimLineDistance = 5000.0f;

	void InitSeatedAtSeat(ASeatActor* TargetSeat);

	UFUNCTION(Client, Reliable)
	void Client_InitSeatedAtSeat(FVector TargetLocation, FRotator TargetRotation);

	UFUNCTION(Client, Reliable)
	void Client_ShowReadyWidget();

	UFUNCTION(Client, Reliable)
	void Client_HideReadyWidget();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnRep_PlayerState() override;

	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraComponent;

	// ��ȣ�ۿ� �Է� ó�� �Լ�
	void OnInteract(const FInputActionValue& Value);

	// ������ ��ȣ�ۿ��� ��û�ϴ� RPC
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* InteractableActor);

	// ���ø����̼�(����ȭ) ����
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ��Ÿ�ְ� ������ �� �������� ȣ��� �ݹ� �Լ�
	UFUNCTION()
	void OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// �� ��� ��Ÿ�ְ� ������ �� �������� ȣ��� �ݹ� �Լ�
	UFUNCTION()
	void OnGrabGunMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnPutBackGunMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	// ���� ��ȣ�ۿ� ���� ���� ĳ��
	UPROPERTY()
	TObjectPtr<ASeatActor> CurrentSeat;

	EGunHoldReason FinishedReason;

	// ���ؼ� ǥ��/����
	void SetMainShotAimLineVisible(bool bVisible);

	// ���ؼ� �׸���
	void DrawMainShotAimLine();

	void CacheCameraComponentFromBlueprint();

	void ShowReadyWidget();
	void HideReadyWidget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> ReadyWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UReadyWidget> ReadyWidgetClass;

};
