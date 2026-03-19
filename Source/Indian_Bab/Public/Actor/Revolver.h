#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Revolver.generated.h"

class USkeletalMeshComponent;
class USphereComponent;

UCLASS()
class INDIAN_BAB_API ARevolver : public AActor
{
	GENERATED_BODY()

public:
	// 생성자: 기본값들을 초기화하는 곳입니다.
	ARevolver();

	// --- 컴포넌트 ---

	// 캡슐 콜리전 컴포넌트 (충돌 감지용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	// 무기의 시각적 모델 (스켈레탈 메시)
	// 블루프린트에서 리볼버 메시를 여기에 할당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* WeaponMesh;


	// --- 무기 스탯 변수 ---

	// 최대 장탄수 (리볼버니까 기본 6발이 적당하겠죠?)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
	int32 MaxAmmo;

	// 현재 장탄수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Stats")
	int32 CurrentAmmo;

	// 무기 데미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
	float Damage;


	// --- 무기 기능(함수) ---

	// 격발 함수 (블루프린트에서도 부를 수 있도록 BlueprintCallable을 붙입니다)
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void Fire();

	// 장전 함수
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void Reload();

protected:
	// 게임이 시작되거나 이 무기가 스폰될 때 호출됩니다.
	virtual void BeginPlay() override;

};
