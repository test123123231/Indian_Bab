#include "Character/LobbyCharacter.h"
#include "EnhancedInputComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "GroomComponent.h"
#include "Interface/InteractableInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "Actor/SeatActor.h"
#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
ALobbyCharacter::ALobbyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bIsSitting = false; // 기본값은 서 있는 상태

	// 1인칭 메타휴먼 바디 생성 및 설정
	FirstPersonMetaHumanBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person MetaHuman Body"));
	FirstPersonMetaHumanBody->SetupAttachment(GetMesh());
	
	FirstPersonMetaHumanBody->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMetaHumanBody->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	// 1인칭 메타휴먼 얼굴 생성 및 설정
	FirstPersonMetaHumanTorso = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person MetaHuman Torso"));
	FirstPersonMetaHumanTorso->SetupAttachment(FirstPersonMetaHumanBody);
	
	FirstPersonMetaHumanTorso->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMetaHumanTorso->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	// 3인칭 메타휴먼 바디 생성 및 설정
	ThirdPersonMetaHumanBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Body"));
	ThirdPersonMetaHumanBody->SetupAttachment(GetMesh());
	ThirdPersonMetaHumanBody->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 메타휴먼 토르소 생성 및 설정
	ThirdPersonMetaHumanTorso = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Torso"));
	ThirdPersonMetaHumanTorso->SetupAttachment(ThirdPersonMetaHumanBody);
	ThirdPersonMetaHumanTorso->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 메타휴먼 얼굴 생성 및 설정
	ThirdPersonMetaHumanFace = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Face"));
	ThirdPersonMetaHumanFace->SetupAttachment(ThirdPersonMetaHumanBody);
	ThirdPersonMetaHumanFace->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 얼굴에 Groom(Hair, Eyebrows, Beard, Mustache, Eyelashes, Fuzz) 컴포넌트 추가
	(ThirdPersonMetaHumanHair = CreateDefaultSubobject< UGroomComponent>(TEXT("Hair")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanEyebrows = CreateDefaultSubobject< UGroomComponent>(TEXT("Eyebrows")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanBeard = CreateDefaultSubobject< UGroomComponent>(TEXT("Beard")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanMustache = CreateDefaultSubobject< UGroomComponent>(TEXT("Mustache")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanEyelashes = CreateDefaultSubobject< UGroomComponent>(TEXT("Eyelashes")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanFuzz = CreateDefaultSubobject< UGroomComponent>(TEXT("Fuzz")))->SetupAttachment(ThirdPersonMetaHumanFace);

	// Create the Camera Component	
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	CameraComponent->SetupAttachment(FirstPersonMetaHumanBody, FName("head"));
	CameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 8.5, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	CameraComponent->bUsePawnControlRotation = true;
	CameraComponent->bEnableFirstPersonFieldOfView = true;
	CameraComponent->bEnableFirstPersonScale = true;
	CameraComponent->FirstPersonFieldOfView = 70.0f;
	CameraComponent->FirstPersonScale = 0.6f;
}


// Called when the game starts or when spawned
void ALobbyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocallyControlled())
	{
		return;
	}
	
	MainGamePC = Cast<AMainGamePlayerController>(GetController());

	FirstPersonMetaHumanBody->SetOnlyOwnerSee(true);
	FirstPersonMetaHumanTorso->SetOnlyOwnerSee(true);

	ThirdPersonMetaHumanBody->SetOwnerNoSee(true);
	ThirdPersonMetaHumanTorso->SetOwnerNoSee(true);
	ThirdPersonMetaHumanFace->SetOwnerNoSee(true);

	ThirdPersonMetaHumanHair->SetOwnerNoSee(true);
	ThirdPersonMetaHumanEyebrows->SetOwnerNoSee(true);
	ThirdPersonMetaHumanBeard->SetOwnerNoSee(true);
	ThirdPersonMetaHumanMustache->SetOwnerNoSee(true);
	ThirdPersonMetaHumanEyelashes->SetOwnerNoSee(true);
	ThirdPersonMetaHumanFuzz->SetOwnerNoSee(true);

	ThirdPersonMetaHumanHair->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanEyebrows->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanBeard->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanMustache->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanEyelashes->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanFuzz->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

}


// Called every frame
void ALobbyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


// Called to bind functionality to input
void ALobbyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Interact)
		{
			EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Triggered, this, &ALobbyCharacter::OnInteract);
		}
	}
}


void ALobbyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// bIsSitting 변수를 멀티플레이 환경에서 동기화
	DOREPLIFETIME(ALobbyCharacter, bIsSitting);
}


void ALobbyCharacter::OnInteract(const FInputActionValue& Value)
{
	// 카메라 위치에서 시선 방향으로 레이캐스트 쏘기
	FVector StartLoc = CameraComponent->GetComponentLocation();
	FVector EndLoc = StartLoc + (CameraComponent->GetForwardVector() * InteractRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 자기 자신은 무시

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		// 맞은 액터가 IInteractableInterface를 상속받았는지 확인!
		if (HitActor && HitActor->Implements<UInteractableInterface>())
		{
			// 서버에 상호작용 요청
			ServerInteract(HitActor);
		}
	}
}


void ALobbyCharacter::ServerInteract_Implementation(AActor* InteractableActor)
{
	// 서버에서 인터페이스의 Interact 함수 실행
	if (InteractableActor && InteractableActor->Implements<UInteractableInterface>())
	{
		IInteractableInterface::Execute_Interact(InteractableActor, this);
	}
}


void ALobbyCharacter::SetSittingState(bool bSitting)
{
	if (HasAuthority())
	{
		bIsSitting = bSitting;
		OnRep_IsSitting(); // 서버 자신도 시야/회전 제한이 즉각 적용되도록 수동 호출
	}
}


void ALobbyCharacter::MulticastPlaySitAnimation_Implementation()
{
	// 3인칭 메타휴먼 바디에 애니메이션 몽타주가 할당되어 있다면 재생
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (SitMontage && AnimInstance)
	{
		// 몽타주 재생 (애니메이션 BP에 'DefaultSlot' 등의 슬롯 설정이 되어 있어야 함)
		AnimInstance->Montage_Play(SitMontage, 1.0f);
	}
}


void ALobbyCharacter::StartSitTransition(ASeatActor* TargetSeat)
{
	CurrentSeat = TargetSeat;

	if (HasAuthority() && SitMontage)
	{
		// 서버에서만 몽타주 종료 시점을 캐치하도록 델리게이트 바인딩
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			// 혹시 이미 바인딩되어 있다면 제거 후 추가
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
			MainAnimInstance->OnMontageEnded.AddDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
		}
	}
}


void ALobbyCharacter::OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 앉기 몽타주가 끝났다면 (서버에서만 실행됨)
	if (Montage == SitMontage && HasAuthority())
	{
		// 루트 모션으로 인해 네트워크 오차가 발생했을 수 있으므로 최종 SitTarget 위치로 캡슐을 강제 보정(Snap)
		if (CurrentSeat && CurrentSeat->SitTarget)
		{
			//SetActorLocationAndRotation(CurrentSeat->SitTarget->GetComponentLocation(), CurrentSeat->SitTarget->GetComponentRotation());
			Client_LockCameraAfterSit(CurrentSeat->SitTarget->GetComponentRotation());
		}

		// 완벽하게 안착했으므로 무브먼트 컴포넌트를 비활성화
		GetCharacterMovement()->DisableMovement();

		// 델리게이트 해제 (메모리 릭 방지)
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
		}

		bIsSittingEnded = true; // 앉기 애니메이션이 완전히 끝났음을 표시하는 플래그
	}
}


void ALobbyCharacter::OnRep_IsSitting()
{
	// 캡슐(몸통) 전체가 마우스를 따라 도는 것을 막습니다.
	bUseControllerRotationYaw = !bIsSitting;

	if (IsLocallyControlled())
	{
		if (bIsSitting)
		{
			// 앉는 애니메이션이 재생되는 동안 카메라는 마우스를 무시하고 머리 뼈(head)를 따라가며 돌아앉는 연출을 보여줍니다.
			CameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 8.5, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
			CameraComponent->bUsePawnControlRotation = false;
		}
		else
		{
			// 일어섰을 때 초기화 (제한 완벽 해제)
			CameraComponent->bUsePawnControlRotation = true;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
				{
					CamManager->ViewYawMin = 0.0f;
					CamManager->ViewYawMax = 359.999f;
					CamManager->ViewPitchMin = -70.0f;
					CamManager->ViewPitchMax = 80.0f;
				}
			}
		}
	}
}


void ALobbyCharacter::Client_LockCameraAfterSit_Implementation(FRotator FinalSitRotation)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 애니메이션이 끝난 바로 그 순간의 '실제 카메라가 바라보는 앞방향(Forward Vector)'을 추출하여 회전값으로 변환합니다.
		// GetComponentRotation()을 그대로 쓰면 카메라에 적용된 상대 회전값(Roll -90, Yaw 90) 때문에 ControlRotation 적용 시 축이 90도 꼬여버립니다.
		FRotator CurrentCameraRot = CameraComponent->GetForwardVector().Rotation();

		// 마우스 컨트롤(ControlRotation)을 현재 카메라가 보고 있는 방향으로 완벽하게 덮어씌웁니다.
		// 이렇게 하면 애니메이션에서 마우스로 조작 권한이 넘어갈 때 화면이 단 1픽셀도 튀지 않습니다!
		PC->SetControlRotation(CurrentCameraRot);

		// 다시 마우스로 카메라를 움직일 수 있도록 활성화
		CameraComponent->bUsePawnControlRotation = true;

		// 시야각 제한 (최종 안착 방향 기준 좌/우 60도)
		// 주의: 시야 제한의 기준점(CenterYaw)은 카메라 방향이 아니라, '의자의 정면(FinalSitRotation)'으로 잡아야
		// 유저가 책상을 기준으로 좌우 동일한 각도로 둘러볼 수 있습니다.
		if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
		{
			float CenterYaw = FinalSitRotation.Yaw;

			// 언리얼 카메라 매니저 버그 방지 (0~360 사이 값으로 정규화)
			float MinYaw = FMath::Fmod(CenterYaw - 60.0f + 360.0f, 360.0f);
			float MaxYaw = FMath::Fmod(CenterYaw + 60.0f + 360.0f, 360.0f);

			CamManager->ViewYawMin = MinYaw;
			CamManager->ViewYawMax = MaxYaw;
			CamManager->ViewPitchMin = -45.0f;
			CamManager->ViewPitchMax = 45.0f;
		}
	}
}


void ALobbyCharacter::Client_PrepareSit_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	// 서버의 복제 딜레이를 기다리지 않고, 클라이언트 스스로 즉시 의자 앞으로 캡슐을 강제 회전 및 이동시킵니다!
	SetActorLocationAndRotation(TargetLocation, TargetRotation);
}