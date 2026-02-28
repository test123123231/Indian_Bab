#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "SeatActor.generated.h"


UCLASS()
class INDIAN_BAB_API ASeatActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ASeatActor();

	// 리플리케이션 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IInteractableInterface 구현부
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() override;

	// 플레이어가 앉았을 때 위치할 정확한 트랜스폼(위치/회전)을 잡아주는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SitTarget;

	// 루트 모션 애니메이션이 시작될 위치 (의자 바로 앞)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> StandTarget;

protected:
	virtual void BeginPlay() override;

private:
	// 의자 상태가 변했을 때(앉았을 때/일어났을 때) 처리할 로직
	UFUNCTION()
	void OnRep_IsOccupied();

	// 의자 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SeatMesh;

	// 이 의자에 누군가 이미 앉아있는지 여부 (서버와 클라이언트 동기화)
	UPROPERTY(ReplicatedUsing = OnRep_IsOccupied)
	bool bIsOccupied;

	// 의자 주인이 누구인지 저장
	UPROPERTY(Replicated)
	TObjectPtr<AActor> Occupant;
};
