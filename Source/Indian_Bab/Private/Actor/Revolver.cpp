#include "Actor/Revolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"

// 생성자
ARevolver::ARevolver()
{
	// 매 프레임마다 Tick()을 호출할 필요가 없다면 false로 꺼두는 것이 게임 성능에 좋습니다.
	PrimaryActorTick.bCanEverTick = false;

	// 멀티플레이어 동기화 필수 - 이게 없으면 클라이언트에서 DeskRevolver 포인터가 null이 됨
	bReplicates = true;

	// 스켈레탈 메시 컴포넌트 생성 및 루트(기준점)로 설정
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh; // 스켈레탈 메시를 루트로 설정

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionSphere->SetupAttachment(RootComponent); // 스켈레탈 메시를 캡슐 콜리전 컴포넌트에 붙입니다.
	
	// 기본 스탯 초기화
	MaxAmmo = 6;
	CurrentAmmo = MaxAmmo;
	Damage = 50.0f;
}

// BeginPlay
void ARevolver::BeginPlay()
{
	Super::BeginPlay();

	// 혹시 모르니 게임 시작 시 탄창을 꽉 채워줍니다.
	CurrentAmmo = MaxAmmo;
}

// 격발 기능
void ARevolver::Fire()
{
	// 잔탄이 남아있을 때만 발사
	if (CurrentAmmo > 0)
	{
		CurrentAmmo--;

		// 화면 좌측 상단과 출력 로그에 테스트용 텍스트를 띄웁니다.
		UE_LOG(LogTemp, Warning, TEXT("Bang! Fired a shot. Ammo left: %d"), CurrentAmmo);

		// TODO: 라인트레이스(LineTrace)를 통한 피격 판정 로직 추가
		// TODO: 총구 화염(Muzzle Flash) 파티클 및 총소리 재생
		// TODO: 리볼버 해머/실린더가 돌아가는 '무기 자체 애니메이션' 재생
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Click. Out of ammo!"));
		// TODO: 찰칵거리는 빈 총소리 사운드 재생
	}
}

// 장전 기능
void ARevolver::Reload()
{
	UE_LOG(LogTemp, Warning, TEXT("Reloading..."));

	// 탄창을 다시 꽉 채웁니다.
	CurrentAmmo = MaxAmmo;

	// TODO: 캐릭터의 재장전 애니메이션 및 리볼버 실린더가 열리는 애니메이션/사운드 재생
}

