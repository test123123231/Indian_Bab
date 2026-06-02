#include "Character/LobbyVRCharacter.h"

#include "Actor/SeatActor.h"
#include "Game/MainGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "MotionControllerComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Widget/GameResultWidget.h"
#include "Widget/ReadyWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"

ALobbyVRCharacter::ALobbyVRCharacter()
{
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(GetRootComponent());

	if (CameraComponent)
	{
		CameraComponent->SetupAttachment(VROrigin);
		CameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->bUsePawnControlRotation = false;
		CameraComponent->bLockToHmd = true;
	}

	MotionControllerRightGrip = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightGrip"));
	MotionControllerRightGrip->SetupAttachment(VROrigin);
	MotionControllerRightGrip->MotionSource = FName(TEXT("RightGrip"));

	MotionControllerLeftGrip = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftGrip"));
	MotionControllerLeftGrip->SetupAttachment(VROrigin);
	MotionControllerLeftGrip->MotionSource = FName(TEXT("LeftGrip"));

	MotionControllerRightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightAim"));
	MotionControllerRightAim->SetupAttachment(VROrigin);
	MotionControllerRightAim->MotionSource = FName(TEXT("RightAim"));

	MotionControllerLeftAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftAim"));
	MotionControllerLeftAim->SetupAttachment(VROrigin);
	MotionControllerLeftAim->MotionSource = FName(TEXT("LeftAim"));

	HandRight = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandRight"));
	HandRight->SetupAttachment(MotionControllerRightGrip);
	HandRight->SetCollisionProfileName(TEXT("NoCollision"));
	HandRight->SetOnlyOwnerSee(true);

	HandLeft = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandLeft"));
	HandLeft->SetupAttachment(MotionControllerLeftGrip);
	HandLeft->SetCollisionProfileName(TEXT("NoCollision"));
	HandLeft->SetOnlyOwnerSee(true);

	WidgetInteractionRight = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionRight"));
	WidgetInteractionRight->SetupAttachment(MotionControllerRightAim);

	WidgetInteractionLeft = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionLeft"));
	WidgetInteractionLeft->SetupAttachment(MotionControllerLeftAim);

	ReadyWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ReadyWidget"));
	ReadyWidgetComponent->SetupAttachment(CameraComponent);
	ReadyWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ReadyWidgetComponent->SetTwoSided(true);
	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReadyWidgetComponent->SetVisibility(false);
	ReadyWidgetComponent->SetHiddenInGame(true);

	ResultWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResultWidget"));
	ResultWidgetComponent->SetupAttachment(CameraComponent);
	ResultWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ResultWidgetComponent->SetTwoSided(true);
	ResultWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResultWidgetComponent->SetVisibility(false);
	ResultWidgetComponent->SetHiddenInGame(true);

	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}


void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void ALobbyVRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
}

void ALobbyVRCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
}

void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVRPointers();
	UpdateArmPosition();
}

void ALobbyVRCharacter::InitSeatedAtSeat(ASeatActor* TargetSeat)
{
	if (!HasAuthority() || !TargetSeat || !TargetSeat->SitTarget)
	{
		return;
	}

	DeskRevolver = TargetSeat->DeskRevolver;

	const FVector OriginalSitLocation = TargetSeat->SitTarget->GetComponentLocation();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	const float CapsuleSeatOffset = bUseCapsuleHalfHeightSeatOffset ? CapsuleHalfHeight : 0.0f;

	FVector SitLocation = OriginalSitLocation;
	SitLocation.Z += CapsuleSeatOffset + SeatHeightOffset;

	FRotator SitRotation = TargetSeat->SitTarget->GetComponentRotation();
	SitRotation.Pitch = 0.0f;
	SitRotation.Roll = 0.0f;

	SetActorLocationAndRotation(SitLocation, SitRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogTemp, Warning,
		TEXT("[LobbyVRCharacter] AutoSeat Seat=%s OriginalSitLocation=%s CapsuleHalfHeight=%.2f bUseCapsuleHalfHeight=%s SeatHeightOffset=%.2f FinalSitLocation=%s AppliedActorLocation=%s"),
		*GetNameSafe(TargetSeat),
		*OriginalSitLocation.ToString(),
		CapsuleHalfHeight,
		bUseCapsuleHalfHeightSeatOffset ? TEXT("true") : TEXT("false"),
		SeatHeightOffset,
		*SitLocation.ToString(),
		*GetActorLocation().ToString());

	bIsSitting = true;
	bIsSittingEnded = true;
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	DrawSeatDebugCapsule();

	Client_InitSeatedAtSeat(SitLocation, SitRotation);
	ForceNetUpdate();
}

void ALobbyVRCharacter::Client_InitSeatedAtSeat_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;

	SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);

	bIsSitting = true;
	bIsSittingEnded = true;
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	DrawSeatDebugCapsule();

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.0f, EOrientPositionSelector::OrientationAndPosition);
	}

	ShowReadyWidgetAfterDelay();
}

void ALobbyVRCharacter::Client_ShowReadyWidget_Implementation()
{
	ShowReadyWidgetAfterDelay();
}

void ALobbyVRCharacter::Client_HideReadyWidget_Implementation()
{
	HideReadyWidget();
}

void ALobbyVRCharacter::Client_ShowResultWidget_Implementation(const FString& WinnerName, int32 WinnerPlayerId)
{
	ShowResultWidget(WinnerName, WinnerPlayerId);
}

void ALobbyVRCharacter::PressRightWidgetInteraction()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right trigger press"));
	if (!WidgetInteractionRight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] WidgetInteractionRight is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right hovered widget: %s"), *GetNameSafe(WidgetInteractionRight->GetHoveredWidgetComponent()));
	const FHitResult& LastHitResult = WidgetInteractionRight->GetLastHitResult();
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right last hit actor: %s component: %s blocking: %s distance: %.2f"),
		*GetNameSafe(LastHitResult.GetActor()),
		*GetNameSafe(LastHitResult.GetComponent()),
		LastHitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
		LastHitResult.Distance);
	WidgetInteractionRight->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseRightWidgetInteraction()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right trigger release"));
	if (!WidgetInteractionRight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] WidgetInteractionRight is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right hovered widget: %s"), *GetNameSafe(WidgetInteractionRight->GetHoveredWidgetComponent()));
	const FHitResult& LastHitResult = WidgetInteractionRight->GetLastHitResult();
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Right last hit actor: %s component: %s blocking: %s distance: %.2f"),
		*GetNameSafe(LastHitResult.GetActor()),
		*GetNameSafe(LastHitResult.GetComponent()),
		LastHitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
		LastHitResult.Distance);
	const bool bReadyWidgetHovered = WidgetInteractionRight->GetHoveredWidgetComponent() == ReadyWidgetComponent;
	const bool bResultWidgetHovered = WidgetInteractionRight->GetHoveredWidgetComponent() == ResultWidgetComponent;
	WidgetInteractionRight->ReleasePointerKey(EKeys::LeftMouseButton);
	if (bReadyWidgetHovered)
	{
		if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent ? ReadyWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ReadyWidget->ConfirmReady();
		}
	}
	if (bResultWidgetHovered)
	{
		if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent ? ResultWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ResultWidget->ConfirmBackToMainMenu();
		}
	}
}

void ALobbyVRCharacter::PressLeftWidgetInteraction()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left trigger press"));
	if (!WidgetInteractionLeft)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] WidgetInteractionLeft is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left hovered widget: %s"), *GetNameSafe(WidgetInteractionLeft->GetHoveredWidgetComponent()));
	const FHitResult& LastHitResult = WidgetInteractionLeft->GetLastHitResult();
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left last hit actor: %s component: %s blocking: %s distance: %.2f"),
		*GetNameSafe(LastHitResult.GetActor()),
		*GetNameSafe(LastHitResult.GetComponent()),
		LastHitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
		LastHitResult.Distance);
	WidgetInteractionLeft->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseLeftWidgetInteraction()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left trigger release"));
	if (!WidgetInteractionLeft)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] WidgetInteractionLeft is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left hovered widget: %s"), *GetNameSafe(WidgetInteractionLeft->GetHoveredWidgetComponent()));
	const FHitResult& LastHitResult = WidgetInteractionLeft->GetLastHitResult();
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Left last hit actor: %s component: %s blocking: %s distance: %.2f"),
		*GetNameSafe(LastHitResult.GetActor()),
		*GetNameSafe(LastHitResult.GetComponent()),
		LastHitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
		LastHitResult.Distance);
	const bool bReadyWidgetHovered = WidgetInteractionLeft->GetHoveredWidgetComponent() == ReadyWidgetComponent;
	const bool bResultWidgetHovered = WidgetInteractionLeft->GetHoveredWidgetComponent() == ResultWidgetComponent;
	WidgetInteractionLeft->ReleasePointerKey(EKeys::LeftMouseButton);
	if (bReadyWidgetHovered)
	{
		if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent ? ReadyWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ReadyWidget->ConfirmReady();
		}
	}
	if (bResultWidgetHovered)
	{
		if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent ? ResultWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ResultWidget->ConfirmBackToMainMenu();
		}
	}
}

void ALobbyVRCharacter::OnRep_IsSitting()
{
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::Server_UpdateArm_Implementation(const FTransform& NewLeftArm, const FTransform& NewRightArm)
{
	LeftArm = NewLeftArm;
	RightArm = NewRightArm;
}

void ALobbyVRCharacter::UpdateAimFromView()
{
	if (!bIsSitting || !IsLocallyControlled() || !CameraComponent)
	{
		return;
	}

	FRotator Forward = CameraComponent->GetComponentRotation();
	
	const FRotator Aim = UKismetMathLibrary::NormalizedDeltaRotator(GetActorRotation(), Forward);
	ReplicatedAim = Aim;
	Server_UpdateAim(ReplicatedAim);
}

void ALobbyVRCharacter::UpdateArmPosition() {
	if (!IsLocallyControlled() || !MotionControllerLeftGrip || !MotionControllerRightGrip)
	{
		return;
	}
	LeftArm = MotionControllerLeftGrip->GetComponentTransform();
	LeftArm = MotionControllerRightGrip->GetComponentTransform();
	Server_UpdateArm(LeftArm, RightArm);
}

void ALobbyVRCharacter::ConfigureVRSeatedState()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (bIsSitting)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->DisableMovement();
			Movement->StopMovementImmediately();
		}
	}

	if (CameraComponent)
	{
		if (CameraComponent->GetAttachParent() != VROrigin)
		{
			CameraComponent->AttachToComponent(VROrigin, FAttachmentTransformRules::KeepRelativeTransform);
		}

		CameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->bUsePawnControlRotation = false;
		CameraComponent->bLockToHmd = true;
	}
}

void ALobbyVRCharacter::ConfigureWidgetInteraction()
{
	int32 VirtualUserIndex = 0;
	int32 RightPointerIndex = 0;
	int32 LeftPointerIndex = 1;

	if (WidgetInteractionRight)
	{
		WidgetInteractionRight->InteractionDistance = VRPointerMaxDistance;
		WidgetInteractionRight->TraceChannel = ECC_Visibility;
		WidgetInteractionRight->bShowDebug = bShowWidgetInteractionDebug;
		WidgetInteractionRight->bEnableHitTesting = true;
		WidgetInteractionRight->InteractionSource = EWidgetInteractionSource::World;
		WidgetInteractionRight->VirtualUserIndex = VirtualUserIndex;
		WidgetInteractionRight->PointerIndex = RightPointerIndex;
	}



	if (WidgetInteractionLeft)
	{
		WidgetInteractionLeft->InteractionDistance = VRPointerMaxDistance;
		WidgetInteractionLeft->TraceChannel = ECC_Visibility;
		WidgetInteractionLeft->bShowDebug = bShowWidgetInteractionDebug;
		WidgetInteractionLeft->bEnableHitTesting = true;
		WidgetInteractionLeft->InteractionSource = EWidgetInteractionSource::World;
		WidgetInteractionLeft->VirtualUserIndex = VirtualUserIndex;
		WidgetInteractionLeft->PointerIndex = LeftPointerIndex;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] WidgetInteraction configured. Character=%s VirtualUser=%d RightPointer=%d LeftPointer=%d"),
		*GetNameSafe(this),
		VirtualUserIndex,
		RightPointerIndex,
		LeftPointerIndex);
}

void ALobbyVRCharacter::ShowReadyWidgetAfterDelay()
{
	if (!IsLocallyControlled()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	World->GetTimerManager().ClearTimer(ReadyWidgetDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		ReadyWidgetDelayTimerHandle,
		this,
		&ALobbyVRCharacter::ShowReadyWidget,
		FMath::Max(ReadyWidgetDelaySeconds, 0.0f),
		false);
}

void ALobbyVRCharacter::UpdateVRPointers()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	UpdateLaserPointer(MotionControllerRightAim, TEXT("Right"));
	UpdateLaserPointer(MotionControllerLeftAim, TEXT("Left"));
}

void ALobbyVRCharacter::UpdateLaserPointer(const UMotionControllerComponent* AimController, const TCHAR* PointerName) const
{
	if (!AimController || !GetWorld() || !bShowVRPointer)
	{
		return;
	}

	const FVector Start = AimController->GetComponentLocation();
	const FVector TraceEnd = Start + AimController->GetForwardVector() * VRPointerMaxDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRPointerTrace), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, TraceEnd, ECC_Visibility, QueryParams);
	const FVector End = bHit ? HitResult.ImpactPoint : TraceEnd;

	const FColor LineColor = FColor(80, 220, 255);
	DrawDebugLine(
		GetWorld(),
		Start,
		End,
		LineColor,
		false,
		0.0f,
		0,
		LaserThickness);

	if (bLogVRPointerHits && bHit)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[LobbyVRCharacter] %s pointer hit actor: %s component: %s blocking: %s distance: %.2f"),
			PointerName,
			*GetNameSafe(HitResult.GetActor()),
			*GetNameSafe(HitResult.GetComponent()),
			HitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
			HitResult.Distance);
	}
}

void ALobbyVRCharacter::DrawSeatDebugCapsule() const
{
	if (!bShowSeatDebugCapsule || !GetWorld())
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	DrawDebugCapsule(
		GetWorld(),
		Capsule->GetComponentLocation(),
		Capsule->GetScaledCapsuleHalfHeight(),
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetComponentQuat(),
		FColor::Green,
		false,
		DebugSeatCapsuleDuration,
		0,
		2.0f);
}

void ALobbyVRCharacter::ShowReadyWidget()
{
	if (!IsLocallyControlled() || !ReadyWidgetComponent) return;

	ConfigureWidgetInteraction();

	if (ReadyWidgetClass)
	{
		ReadyWidgetComponent->SetWidgetClass(ReadyWidgetClass);
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController()) return;

	ReadyWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	ReadyWidgetComponent->InitWidget();

	if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent->GetUserWidgetObject()))
	{
		ReadyWidget->SetOwningPlayer(PC);
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget owning player set to %s"), *GetNameSafe(PC));
	}

	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReadyWidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReadyWidgetComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ReadyWidgetComponent->SetHiddenInGame(false);
	ReadyWidgetComponent->SetVisibility(true, true);

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget shown. Local=%s Collision=%d WidgetClass=%s WidgetObject=%s"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ReadyWidgetComponent->GetCollisionEnabled()),
		*GetNameSafe(ReadyWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ReadyWidgetComponent->GetUserWidgetObject()));
}

void ALobbyVRCharacter::HideReadyWidget()
{
	if (!ReadyWidgetComponent) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReadyWidgetDelayTimerHandle);
	}

	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReadyWidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReadyWidgetComponent->SetGenerateOverlapEvents(false);
	ReadyWidgetComponent->SetVisibility(false);
	ReadyWidgetComponent->SetHiddenInGame(true);


	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget hidden"));
}

void ALobbyVRCharacter::ShowResultWidget(const FString& WinnerName, int32 WinnerPlayerId)
{
	if (!IsLocallyControlled() || !ResultWidgetComponent) return;

	HideReadyWidget();
	ConfigureWidgetInteraction();


	if (ResultWidgetClass)
	{
		ResultWidgetComponent->SetWidgetClass(ResultWidgetClass);
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController()) return;

	ResultWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	ResultWidgetComponent->InitWidget();

	if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent->GetUserWidgetObject()))
	{
		ResultWidget->SetOwningPlayer(PC);
		ResultWidget->SetResult(WinnerName, PC->GetPlayerIdSafe() == WinnerPlayerId);
	}

	ResultWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResultWidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ResultWidgetComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ResultWidgetComponent->SetHiddenInGame(false);
	ResultWidgetComponent->SetVisibility(true, true);

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ResultWidget shown. Winner=%s WidgetClass=%s WidgetObject=%s"),
		*WinnerName,
		*GetNameSafe(ResultWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ResultWidgetComponent->GetUserWidgetObject()));
}
void ALobbyVRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyVRCharacter, LeftArm);
	DOREPLIFETIME(ALobbyVRCharacter, RightArm);
}

void ALobbyVRCharacter::GrabGun() {
	if (GunHoldReason != EGunHoldReason::Win) return;
	if (!MotionControllerRightGrip || !GetWorld())
	{
		return;
	}
	const FVector Start = MotionControllerRightGrip->GetComponentLocation();
	const FVector TraceEnd = Start + MotionControllerRightGrip->GetForwardVector() * VRPointerMaxDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRPointerTrace), false, this);

	UE_LOG(LogTemp, Warning, TEXT("Gun visible"));
	AttachRevolverToSocket();
	DrawMainShotAimLine();

}
