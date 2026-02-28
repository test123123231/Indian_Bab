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
	FirstPersonMetaHumanFace = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person MetaHuman Face"));
	FirstPersonMetaHumanFace->SetupAttachment(FirstPersonMetaHumanBody);
	
	FirstPersonMetaHumanFace->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMetaHumanFace->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

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
	CameraComponent->SetupAttachment(FirstPersonMetaHumanFace, FName("head"));
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
	FirstPersonMetaHumanFace->SetOnlyOwnerSee(true);

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
		// 상태를 Sitting으로 변경 (Replicate 되어 클라이언트의 ABP가 Sitting 상태로 넘어감)
		SetSittingState(true);

		//// 2. 루트 모션으로 인해 네트워크 오차가 발생했을 수 있으므로 최종 SitTarget 위치로 캡슐을 강제 보정(Snap)
		//if (CurrentSeat && CurrentSeat->SitTarget)
		//{
		//	SetActorLocationAndRotation(CurrentSeat->SitTarget->GetComponentLocation(), CurrentSeat->SitTarget->GetComponentRotation());
		//}

		// 3. 완벽하게 안착했으므로 무브먼트 컴포넌트를 비활성화
		GetCharacterMovement()->DisableMovement();

		// 4. 델리게이트 해제 (메모리 릭 방지)
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
		}
	}
}