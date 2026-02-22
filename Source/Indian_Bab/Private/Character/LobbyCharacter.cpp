#include "Character/LobbyCharacter.h"
#include "EnhancedInputComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "GroomComponent.h"

// Sets default values
ALobbyCharacter::ALobbyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

}