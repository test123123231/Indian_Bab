#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LobbyCharacter.generated.h"


class UInputAction;
class AMainGamePlayerController;
struct FInputActionValue;
class UCameraComponent;
class UGroomComponent;
class ASeatActor;


UCLASS()
class INDIAN_BAB_API ALobbyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALobbyCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 서버가 모든 클라이언트에게 애니메이션을 재생하라고 지시하는 함수
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySitAnimation();

	// 앉기 상태 변경 함수 (서버에서 호출)
	void SetSittingState(bool bSitting);

	// 의자 상호작용 시 호출되어 몽타주 종료를 기다립니다.
	void StartSitTransition(ASeatActor* TargetSeat);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMetaHumanBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMetaHumanFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanTorso;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanHair;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanEyebrows;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanBeard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanMustache;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanEyelashes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanFuzz;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CameraComponent;

	// 의자에 앉을 때 재생할 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> SitMontage;

	// ★ 추가: 현재 캐릭터가 앉아있는지 여부 (동기화 됨)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsSitting;

	// 앉기 몽타주가 끝났는지 여부 (서버에서만 관리)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsSittingEnded;

	// 상호작용 거리
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.0f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 상호작용 입력 처리 함수
	void OnInteract(const FInputActionValue& Value);

	// 서버에 상호작용을 요청하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* InteractableActor);

	// 리플리케이션(동기화) 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 몽타주가 끝났을 때 서버에서 호출될 콜백 함수
	UFUNCTION()
	void OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
	TObjectPtr<AMainGamePlayerController> MainGamePC;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	// 현재 상호작용 중인 의자 캐싱
	UPROPERTY()
	TObjectPtr<ASeatActor> CurrentSeat;
};
