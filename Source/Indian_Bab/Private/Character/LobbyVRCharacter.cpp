#include "Character/LobbyVRCharacter.h"
#include "EnhancedInputComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "Interface/InteractableInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "Actor/SeatActor.h"
#include "Actor/Revolver.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/MainGameMode.h"
#include "PlayerState/MainPlayerState.h"
#include "Widget/PlayerNameWidget.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
ALobbyVRCharacter::ALobbyVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bIsSitting = true;

	VRHead = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VRHead"));
	VRHead->SetupAttachment(GetRootComponent());
	VRHead->SetOwnerNoSee(true);

	VRBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VRBody"));
	VRBody->SetupAttachment(GetRootComponent());
	VRBody->SetOwnerNoSee(true);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(GetRootComponent());

	PlayerNameWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PlayerNameWidgetComponent"));
	PlayerNameWidgetComponent->SetupAttachment(GetRootComponent());
	PlayerNameWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	PlayerNameWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	PlayerNameWidgetComponent->SetDrawAtDesiredSize(true);
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocallyControlled()) return;

	//MainGamePC = Cast<AMainGamePlayerController>(GetController());
}

// 서버에서 스팀 닉네임 및 카드 바인딩
void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	BindPlayerStateDelegates();
}

// 스팀 닉네임 및 카드 바인딩 함수(PS에서 스팀 닉네임이나 카드 상태 변화하면 Updaate함수 실행)
void ALobbyVRCharacter::BindPlayerStateDelegates()
{
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	PS->OnSteamNicknameChanged.RemoveAll(this);
	PS->OnSteamNicknameChanged.AddUObject(this, &ALobbyVRCharacter::UpdateNameWidget);

	PS->OnCardChanged.RemoveAll(this);
	PS->OnCardChanged.AddUObject(this, &ALobbyVRCharacter::UpdateCardWidget);

	UpdateNameWidget();
	UpdateCardWidget();
}

void ALobbyVRCharacter::UpdateNameWidget()
{
	if (!PlayerNameWidgetComponent) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(PlayerNameWidgetComponent->GetUserWidgetObject());
	if (!Widget) return;

	if (IsLocallyControlled())
	{
		Widget->SetCardText(TEXT(""));
		return;
	}

	FString Name = PS->GetSteamNickname();
	if (Name.IsEmpty())
	{
		Name = TEXT("Unknown");
	}
	Widget->SetPlayerName(Name);
}

void ALobbyVRCharacter::UpdateCardWidget()
{
	if (!PlayerNameWidgetComponent) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(PlayerNameWidgetComponent->GetUserWidgetObject());
	if (!Widget) return;

	if (IsLocallyControlled())
	{
		Widget->SetCardText(TEXT(""));
		return;
	}

	const FCardData Card = PS->GetMyCard();
	if (Card.Value == 0)
	{
		Widget->SetCardText(TEXT(""));
		return;
	}

	const FString CardStr = FString::Printf(TEXT("%d %s"), Card.Value, *Card.Suit);
	Widget->SetCardText(CardStr);
}

// Called every frame
void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 내가 조종하는 캐릭터이고, 앉아있을 때만 작동
	if (bIsSitting && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// (현재 마우스 좌우 방향) - (의자에 안착한 캡슐의 고정된 방향) = 순수하게 목이 돌아간 각도
			float YawDifference = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, PC->GetControlRotation().Yaw);
			//UE_LOG(LogTemp, Log, TEXT("YawDifference: %f"), YawDifference);
			// 내 화면을 위해 로컬 변수 즉시 업데이트
			ReplicatedAimYaw = YawDifference;

			// 남들도 내 고개 돌아가는 걸 볼 수 있게 서버로 전송
			Server_UpdateAimYaw(YawDifference);
		}
	}

	DrawMainShotAimLine();
}
//**해당함수 고쳐야함

// Called to bind functionality to input
void ALobbyVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Interact)
		{
			EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Triggered, this, &ALobbyVRCharacter::OnInteract);
		}
	}
}


void ALobbyVRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// bIsSitting 변수를 멀티플레이 환경에서 동기화
	DOREPLIFETIME(ALobbyVRCharacter, bIsSitting);
	DOREPLIFETIME(ALobbyVRCharacter, GunHoldReason);
	DOREPLIFETIME(ALobbyVRCharacter, DeskRevolver);
	DOREPLIFETIME(ALobbyVRCharacter, ReplicatedAimYaw);
	DOREPLIFETIME(ALobbyVRCharacter, ActiveRevolver);
}


void ALobbyVRCharacter::OnInteract(const FInputActionValue& Value)
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


void ALobbyVRCharacter::ServerInteract_Implementation(AActor* InteractableActor)
{
	// 서버에서 인터페이스의 Interact 함수 실행
	if (InteractableActor && InteractableActor->Implements<UInteractableInterface>())
	{
		IInteractableInterface::Execute_Interact(InteractableActor, this);
	}
}


void ALobbyVRCharacter::SetSittingState(bool bSitting)
{
	if (HasAuthority())
	{
		bIsSitting = bSitting;
		OnRep_IsSitting(); // 서버 자신도 시야/회전 제한이 즉각 적용되도록 수동 호출
	}
}


void ALobbyVRCharacter::MulticastPlaySitAnimation_Implementation()
{
	// 3인칭 메타휴먼 바디에 애니메이션 몽타주가 할당되어 있다면 재생
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (SitMontage && AnimInstance)
	{
		// 몽타주 재생 (애니메이션 BP에 'DefaultSlot' 등의 슬롯 설정이 되어 있어야 함)
		AnimInstance->Montage_Play(SitMontage, 1.0f);
	}
}

void ALobbyVRCharacter::Multicast_PlayGrabGunMontage_Implementation(EGunHoldReason Reason)
{
	GunHoldReason = Reason;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	UAnimMontage* MontageToPlay = nullptr;
	if (Reason == EGunHoldReason::Fold)
	{
		MontageToPlay = AimMyselfMontage;
	}
	else if (Reason == EGunHoldReason::Win)
	{
		MontageToPlay = WinAimMontage;
	}

	if (MontageToPlay)
	{
		AnimInstance->Montage_Play(MontageToPlay, 1.0f);
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ALobbyVRCharacter::OnGrabGunMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
}

void ALobbyVRCharacter::Multicast_PutBackGunMontage_Implementation(EGunHoldReason Reason)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	bIsPuttingBackGun = true;

	// 메인 리볼버를 내려놓기 시작하면 조준선 숨김
	if (Reason == EGunHoldReason::Win)
	{
		SetMainShotAimLineVisible(false);
	}
	UAnimMontage* MontageToPlay = nullptr;

	if (Reason == EGunHoldReason::Fold)
	{
		MontageToPlay = EndAimMyselfMontage;
	}
	else if (Reason == EGunHoldReason::Win)
	{
		MontageToPlay = WinEndMontage;
	}

	AnimInstance->Montage_Play(MontageToPlay, 1.0f);
	FinishedReason = GunHoldReason;
	GunHoldReason = EGunHoldReason::None;

	if (HasAuthority())
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ALobbyVRCharacter::OnPutBackGunMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
}

void ALobbyVRCharacter::OnRep_GunHoldReason()
{
	// GunHoldReason이 변경되면 ABP가 자동으로 감지해서 스테이트 트랜지션에 활용
}

void ALobbyVRCharacter::StartSitTransition(ASeatActor* TargetSeat)
{
	CurrentSeat = TargetSeat;

	if (HasAuthority() && SitMontage)
	{
		// 서버에서만 몽타주 종료 시점을 캐치하도록 델리게이트 바인딩
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			// 혹시 이미 바인딩되어 있다면 제거 후 추가
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
			MainAnimInstance->OnMontageEnded.AddDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
		}
	}
}


void ALobbyVRCharacter::OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
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
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
		}

		bIsSittingEnded = true; // 앉기 애니메이션이 완전히 끝났음을 표시하는 플래그
	}
}

void ALobbyVRCharacter::OnGrabGunMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (!HasAuthority()) return;
	if (GunHoldReason == EGunHoldReason::Fold)
	{
		AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
		if (!GM) return;

		GM->HandleFoldMontageFinished(this);
	}
	else if (GunHoldReason == EGunHoldReason::Win)
	{
		AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
		if (!GM) return;

		GM->HandleMainMontageFinished(this);
	}
}

void ALobbyVRCharacter::OnPutBackGunMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (!HasAuthority()) return;

	bIsPuttingBackGun = false;

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM) return;

	GM->HandlePutBackGunMontageFinished(this, FinishedReason);
}

void ALobbyVRCharacter::OnRep_IsSitting()
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
			APlayerController* PC = Cast<APlayerController>(GetController());
			PC->SetControlRotation(GetActorRotation()); // 카메라가 현재 몸통이 바라보는 방향으로 즉시 회전하도록 강제
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

void ALobbyVRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindPlayerStateDelegates();
}

void ALobbyVRCharacter::Client_LockCameraAfterSit_Implementation(FRotator FinalSitRotation)
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

void SetSittingState(bool bSitting);

void ALobbyVRCharacter::Client_PrepareSit_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	// 서버의 복제 딜레이를 기다리지 않고, 클라이언트 스스로 즉시 의자 앞으로 캡슐을 강제 회전 및 이동시킵니다!
	SetActorLocationAndRotation(TargetLocation, TargetRotation);
}


void ALobbyVRCharacter::Server_UpdateAimYaw_Implementation(float NewYaw)
{
	ReplicatedAimYaw = NewYaw; // 서버가 값을 받아서 모든 클라이언트에게 자동 전파
}

void ALobbyVRCharacter::SetActiveRevolver(ARevolver* NewRevolver)
{
	if (!HasAuthority()) return;

	ActiveRevolver = NewRevolver;
	ForceNetUpdate();
}

void ALobbyVRCharacter::SetMainShotAimLineVisible(bool bVisible)
{
	bShowMainShotAimLine = bVisible;
}

void ALobbyVRCharacter::DrawMainShotAimLine()
{
	// 자기 화면에서만 보이게
	if (!IsLocallyControlled()) return;

	// 메인 리볼버 조준 중일 때만
	if (!bShowMainShotAimLine) return;
	if (GunHoldReason != EGunHoldReason::Win) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Forward = ViewRotation.Vector();
	const FVector Start = ViewLocation + Forward * 80.0f;
	const FVector End = Start + Forward * 2500.0f;

	DrawDebugLine(
		GetWorld(),
		Start,
		End,
		FColor::Red,
		false,
		0.0f,
		0,
		2.0f
	);
}