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
#include "Actor/SeatActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Widget/ReadyWidget.h"
#include "Blueprint/UserWidget.h"

// Sets default values
ALobbyVRCharacter::ALobbyVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bIsSitting = true;

	// VRHead = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VRHead"));
	// VRHead->SetupAttachment(GetRootComponent());
	// VRHead->SetOwnerNoSee(true);

	// VRBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VRBody"));
	// VRBody->SetupAttachment(GetRootComponent());
	// VRBody->SetOwnerNoSee(true);

	// CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	// CameraComponent->SetupAttachment(GetRootComponent());

	PlayerNameWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PlayerNameWidgetComponent"));
	PlayerNameWidgetComponent->SetupAttachment(GetRootComponent());
	PlayerNameWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	PlayerNameWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	PlayerNameWidgetComponent->SetDrawAtDesiredSize(true);

	ReadyWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ReadyWidgetComponent"));
	ReadyWidgetComponent->SetupAttachment(GetRootComponent());
	ReadyWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ReadyWidgetComponent->SetDrawSize(FVector2D(800.0f, 400.0f));
	ReadyWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	ReadyWidgetComponent->SetWorldScale3D(FVector(0.1f, 0.1f, 0.1f));
	ReadyWidgetComponent->SetVisibility(false);
	ReadyWidgetComponent->SetHiddenInGame(true);
	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReadyWidgetComponent->SetGenerateOverlapEvents(false);
	ReadyWidgetComponent->SetTwoSided(true);
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	CacheCameraComponentFromBlueprint();

	if (!CameraComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyVRCharacter] CameraComponent not found in BP: %s"), *GetName());
		return;
	}

	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ReadyWidgetComponent)
			{
				ReadyWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] BeginPlay ReadyWidget initialized"));
}

// �������� ���� �г��� �� ī�� ���ε�
void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	BindPlayerStateDelegates();
}

// ���� �г��� �� ī�� ���ε� �Լ�(PS���� ���� �г����̳� ī�� ���� ��ȭ�ϸ� Updaate�Լ� ����)
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

	// ���� �����ϴ� ĳ�����̰�, �ɾ����� ���� �۵�
	if (bIsSitting && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// (���� ���콺 �¿� ����) - (���ڿ� ������ ĸ���� ������ ����) = �����ϰ� ���� ���ư� ����
			float YawDifference = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, PC->GetControlRotation().Yaw);
			//UE_LOG(LogTemp, Log, TEXT("YawDifference: %f"), YawDifference);
			// �� ȭ���� ���� ���� ���� ��� ������Ʈ
			ReplicatedAimYaw = YawDifference;

			// ���鵵 �� ���� ���ư��� �� �� �� �ְ� ������ ����
			Server_UpdateAimYaw(YawDifference);
		}
	}

	DrawMainShotAimLine();
}
//**�ش��Լ� ���ľ���

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

	// bIsSitting ������ ��Ƽ�÷��� ȯ�濡�� ����ȭ
	DOREPLIFETIME(ALobbyVRCharacter, bIsSitting);
	DOREPLIFETIME(ALobbyVRCharacter, GunHoldReason);
	DOREPLIFETIME(ALobbyVRCharacter, DeskRevolver);
	DOREPLIFETIME(ALobbyVRCharacter, ReplicatedAimYaw);
	DOREPLIFETIME(ALobbyVRCharacter, ActiveRevolver);
}


void ALobbyVRCharacter::OnInteract(const FInputActionValue& Value)
{
	// ī�޶� ��ġ���� �ü� �������� ����ĳ��Ʈ ���
	FVector StartLoc = CameraComponent->GetComponentLocation();
	FVector EndLoc = StartLoc + (CameraComponent->GetForwardVector() * InteractRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // �ڱ� �ڽ��� ����

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		// ���� ���Ͱ� IInteractableInterface�� ��ӹ޾Ҵ��� Ȯ��!
		if (HitActor && HitActor->Implements<UInteractableInterface>())
		{
			// ������ ��ȣ�ۿ� ��û
			ServerInteract(HitActor);
		}
	}
}


void ALobbyVRCharacter::ServerInteract_Implementation(AActor* InteractableActor)
{
	// �������� �������̽��� Interact �Լ� ����
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
		OnRep_IsSitting(); // ���� �ڽŵ� �þ�/ȸ�� ������ �ﰢ ����ǵ��� ���� ȣ��
	}
}


void ALobbyVRCharacter::MulticastPlaySitAnimation_Implementation()
{
	// 3��Ī ��Ÿ�޸� �ٵ� �ִϸ��̼� ��Ÿ�ְ� �Ҵ�Ǿ� �ִٸ� ���
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (SitMontage && AnimInstance)
	{
		// ��Ÿ�� ��� (�ִϸ��̼� BP�� 'DefaultSlot' ���� ���� ������ �Ǿ� �־�� ��)
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

	// ���� �������� �������� �����ϸ� ���ؼ� ����
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
	// GunHoldReason�� ����Ǹ� ABP�� �ڵ����� �����ؼ� ������Ʈ Ʈ�����ǿ� Ȱ��
}

void ALobbyVRCharacter::StartSitTransition(ASeatActor* TargetSeat)
{
	CurrentSeat = TargetSeat;

	if (HasAuthority() && SitMontage)
	{
		// ���������� ��Ÿ�� ���� ������ ĳġ�ϵ��� ��������Ʈ ���ε�
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			// Ȥ�� �̹� ���ε��Ǿ� �ִٸ� ���� �� �߰�
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
			MainAnimInstance->OnMontageEnded.AddDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
		}
	}
}


void ALobbyVRCharacter::OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// �ɱ� ��Ÿ�ְ� �����ٸ� (���������� �����)
	if (Montage == SitMontage && HasAuthority())
	{
		// ��Ʈ ������� ���� ��Ʈ��ũ ������ �߻����� �� �����Ƿ� ���� SitTarget ��ġ�� ĸ���� ���� ����(Snap)
		if (CurrentSeat && CurrentSeat->SitTarget)
		{
			//SetActorLocationAndRotation(CurrentSeat->SitTarget->GetComponentLocation(), CurrentSeat->SitTarget->GetComponentRotation());
			Client_LockCameraAfterSit(CurrentSeat->SitTarget->GetComponentRotation());
		}

		// �Ϻ��ϰ� ���������Ƿ� �����Ʈ ������Ʈ�� ��Ȱ��ȭ
		GetCharacterMovement()->DisableMovement();

		// ��������Ʈ ���� (�޸� �� ����)
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyVRCharacter::OnSitMontageEnded);
		}

		bIsSittingEnded = true; // �ɱ� �ִϸ��̼��� ������ �������� ǥ���ϴ� �÷���
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
	bUseControllerRotationYaw = false;

	if (!CameraComponent)
	{
		CacheCameraComponentFromBlueprint();
	}

	if (bIsSitting)
	{
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->DisableMovement();
		}

		if (CameraComponent)
		{
			CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
			CameraComponent->bUsePawnControlRotation = false;
			CameraComponent->bLockToHmd = true;
		}
	}
	else
	{
		if (CameraComponent)
		{
			CameraComponent->bUsePawnControlRotation = false;
			CameraComponent->bLockToHmd = true;
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
		// �ִϸ��̼��� ���� �ٷ� �� ������ '���� ī�޶� �ٶ󺸴� �չ���(Forward Vector)'�� �����Ͽ� ȸ�������� ��ȯ�մϴ�.
		// GetComponentRotation()�� �״�� ���� ī�޶� ����� ��� ȸ����(Roll -90, Yaw 90) ������ ControlRotation ���� �� ���� 90�� ���������ϴ�.
		FRotator CurrentCameraRot = CameraComponent->GetForwardVector().Rotation();

		// ���콺 ��Ʈ��(ControlRotation)�� ���� ī�޶� ���� �ִ� �������� �Ϻ��ϰ� �����ϴ�.
		// �̷��� �ϸ� �ִϸ��̼ǿ��� ���콺�� ���� ������ �Ѿ �� ȭ���� �� 1�ȼ��� Ƣ�� �ʽ��ϴ�!
		PC->SetControlRotation(CurrentCameraRot);

		// �ٽ� ���콺�� ī�޶� ������ �� �ֵ��� Ȱ��ȭ
		CameraComponent->bUsePawnControlRotation = true;

		// �þ߰� ���� (���� ���� ���� ���� ��/�� 60��)

		if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
		{
			float CenterYaw = FinalSitRotation.Yaw;

			// �𸮾� ī�޶� �Ŵ��� ���� ���� (0~360 ���� ������ ����ȭ)
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
	// ������ ���� �����̸� ��ٸ��� �ʰ�, Ŭ���̾�Ʈ ������ ��� ���� ������ ĸ���� ���� ȸ�� �� �̵���ŵ�ϴ�!
	SetActorLocationAndRotation(TargetLocation, TargetRotation);
}


void ALobbyVRCharacter::Server_UpdateAimYaw_Implementation(float NewYaw)
{
	ReplicatedAimYaw = NewYaw; // ������ ���� �޾Ƽ� ��� Ŭ���̾�Ʈ���� �ڵ� ����
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
	// �ڱ� ȭ�鿡���� ���̰�
	if (!IsLocallyControlled()) return;

	// ���� ������ ���� ���� ����
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

void ALobbyVRCharacter::InitSeatedAtSeat(ASeatActor* TargetSeat)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!TargetSeat || !TargetSeat->SitTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR InitSeatedAtSeat] TargetSeat or SitTarget is null"));
		return;
	}

	FVector SitLocation = TargetSeat->SitTarget->GetComponentLocation();
	FRotator SitRotation = TargetSeat->SitTarget->GetComponentRotation();

	SitRotation.Pitch = 0.0f;
	SitRotation.Roll = 0.0f;

	SetActorLocationAndRotation(
		SitLocation,
		SitRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	bIsSitting = true;
	bIsSittingEnded = true;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}

	OnRep_IsSitting();
	Client_InitSeatedAtSeat(SitLocation, SitRotation);

	ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT("[VR InitSeatedAtSeat] %s seated at %s"),
		*GetName(),
		*GetNameSafe(TargetSeat)
	);
}

void ALobbyVRCharacter::Client_InitSeatedAtSeat_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	SetActorLocationAndRotation(
		TargetLocation,
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	bIsSitting = true;
	bIsSittingEnded = true;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}

	if (!CameraComponent)
	{
		CacheCameraComponentFromBlueprint();
	}

	if (CameraComponent)
	{
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->bUsePawnControlRotation = false;
	}

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(
			0.0f,
			EOrientPositionSelector::OrientationAndPosition
		);
	}

	ShowReadyWidget();

	UE_LOG(LogTemp, Warning, TEXT("[VR Client_InitSeatedAtSeat] %s"), *GetName());
}

void ALobbyVRCharacter::CacheCameraComponentFromBlueprint()
{
	if (CameraComponent)
	{
		return;
	}

	TArray<UCameraComponent*> CameraComponents;
	GetComponents<UCameraComponent>(CameraComponents);

	if (CameraComponents.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyVRCharacter] No CameraComponent found on %s"), *GetName());
		return;
	}

	// 일단 첫 번째 카메라 사용
	CameraComponent = CameraComponents[0];

	UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] Cached CameraComponent: %s"), *GetNameSafe(CameraComponent));
}

void ALobbyVRCharacter::ShowReadyWidget()
{
	if (!IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] Not locally controlled"));
		return;
	}

	if (!ReadyWidgetComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] ReadyWidgetComponent is null"));
		return;
	}

	if (!CameraComponent)
	{
		CacheCameraComponentFromBlueprint();
	}

	if (!CameraComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] CameraComponent is null"));
		return;
	}

	if (ReadyWidgetClass)
	{
		ReadyWidgetComponent->SetWidgetClass(ReadyWidgetClass);
	}

	if (!ReadyWidgetComponent->GetWidgetClass() && !ReadyWidgetComponent->GetWidget())
	{
		UE_LOG(LogTemp, Error, TEXT("[ReadyWidget] WidgetClass and Widget are both null"));
		return;
	}

	ReadyWidgetComponent->InitWidget();

	ReadyWidgetComponent->AttachToComponent(
		CameraComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale
	);

	// 카메라 바로 앞 120cm
	ReadyWidgetComponent->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));

	// 보통 WidgetComponent 앞면 보정
	ReadyWidgetComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	// 너무 커서 안 보이거나 시야를 덮는 문제 방지
	ReadyWidgetComponent->SetDrawSize(FVector2D(800.0f, 400.0f));
	ReadyWidgetComponent->SetWorldScale3D(FVector(0.1f, 0.1f, 0.1f));

	ReadyWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ReadyWidgetComponent->SetTwoSided(true);
	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	ReadyWidgetComponent->SetHiddenInGame(false);
	ReadyWidgetComponent->SetVisibility(true, true);

	UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] Attached to Camera"));
	UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] WidgetClass=%s Widget=%s RelLoc=%s RelRot=%s Hidden=%d Visible=%d"),
		*GetNameSafe(ReadyWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ReadyWidgetComponent->GetWidget()),
		*ReadyWidgetComponent->GetRelativeLocation().ToString(),
		*ReadyWidgetComponent->GetRelativeRotation().ToString(),
		ReadyWidgetComponent->bHiddenInGame,
		ReadyWidgetComponent->IsVisible()
	);
}

void ALobbyVRCharacter::HideReadyWidget()
{
	if (!ReadyWidgetComponent)
	{
		return;
	}

	ReadyWidgetComponent->SetVisibility(false);

	UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] ReadyWidget hidden"));
}

void ALobbyVRCharacter::Client_ShowReadyWidget_Implementation()
{
	ShowReadyWidget();
}

void ALobbyVRCharacter::Client_HideReadyWidget_Implementation()
{
	HideReadyWidget();
}