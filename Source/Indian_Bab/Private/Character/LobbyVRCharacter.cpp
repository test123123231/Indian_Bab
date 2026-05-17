#include "Character/LobbyVRCharacter.h"

#include "Actor/SeatActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MotionControllerComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Widget/ReadyWidget.h"

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
	WidgetInteractionRight->InteractionDistance = 500.0f;
	WidgetInteractionRight->bShowDebug = false;

	ReadyWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ReadyWidget"));
	ReadyWidgetComponent->SetupAttachment(CameraComponent);
	ReadyWidgetComponent->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	ReadyWidgetComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ReadyWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ReadyWidgetComponent->SetDrawSize(FVector2D(800.0f, 400.0f));
	ReadyWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	ReadyWidgetComponent->SetWorldScale3D(FVector(0.1f));
	ReadyWidgetComponent->SetTwoSided(true);
	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ReadyWidgetComponent->SetGenerateOverlapEvents(false);
	ReadyWidgetComponent->SetVisibility(false);
	ReadyWidgetComponent->SetHiddenInGame(true);

	ConfigureVRSeatedState();
	ConfigurePlayerNameWidget();
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureVRSeatedState();
	ConfigurePlayerNameWidget();

	if (ReadyWidgetClass && ReadyWidgetComponent)
	{
		ReadyWidgetComponent->SetWidgetClass(ReadyWidgetClass);
	}

	if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController()))
	{
		if (ReadyWidgetComponent)
		{
			ReadyWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
		}
	}
}

void ALobbyVRCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	ConfigurePlayerNameWidget();
	HideLocalPlayerNameWidget();
}

void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HideLocalPlayerNameWidget();
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
	ConfigurePlayerNameWidget();
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
	ConfigurePlayerNameWidget();
	DrawSeatDebugCapsule();

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.0f, EOrientPositionSelector::OrientationAndPosition);
	}

	// Ready UI is intentionally disabled for the current auto-seating pose/camera test.
}

void ALobbyVRCharacter::Client_ShowReadyWidget_Implementation()
{
	ShowReadyWidget();
}

void ALobbyVRCharacter::Client_HideReadyWidget_Implementation()
{
	HideReadyWidget();
}

void ALobbyVRCharacter::OnRep_IsSitting()
{
	ConfigureVRSeatedState();
	ConfigurePlayerNameWidget();
}

void ALobbyVRCharacter::UpdateAimYawFromView()
{
	if (!bIsSitting || !IsLocallyControlled() || !CameraComponent)
	{
		return;
	}

	FVector Forward = CameraComponent->GetForwardVector();
	Forward.Z = 0.0f;

	if (!Forward.Normalize())
	{
		return;
	}

	const float AimYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Forward.Rotation().Yaw);
	ReplicatedAimYaw = AimYaw;
	Server_UpdateAimYaw(AimYaw);
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

void ALobbyVRCharacter::ConfigurePlayerNameWidget()
{
	if (!NameWidgetComponent)
	{
		return;
	}

	NameWidgetComponent->SetOwnerNoSee(true);

	if (IsLocallyControlled())
	{
		NameWidgetComponent->SetHiddenInGame(true);
		NameWidgetComponent->SetVisibility(false, true);
		return;
	}

	NameWidgetComponent->SetHiddenInGame(false);
	NameWidgetComponent->SetVisibility(true, true);

	if (bAttachPlayerNameWidgetToHeadSocket &&
		ThirdPersonMetaHumanBody &&
		ThirdPersonMetaHumanBody->DoesSocketExist(PlayerNameWidgetHeadSocketName))
	{
		NameWidgetComponent->AttachToComponent(
			ThirdPersonMetaHumanBody,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			PlayerNameWidgetHeadSocketName);
		NameWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, PlayerNameWidgetHeadSocketHeightOffset));
		return;
	}

	NameWidgetComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NameWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, PlayerNameWidgetHeightOffset));
}

void ALobbyVRCharacter::HideLocalPlayerNameWidget()
{
	if (!NameWidgetComponent || !IsLocallyControlled())
	{
		return;
	}

	NameWidgetComponent->SetOwnerNoSee(true);
	NameWidgetComponent->SetHiddenInGame(true);
	NameWidgetComponent->SetVisibility(false, true);
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
	if (!IsLocallyControlled() || !ReadyWidgetComponent)
	{
		return;
	}

	if (ReadyWidgetClass)
	{
		ReadyWidgetComponent->SetWidgetClass(ReadyWidgetClass);
	}

	if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController()))
	{
		ReadyWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	}

	ReadyWidgetComponent->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	ReadyWidgetComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ReadyWidgetComponent->InitWidget();
	ReadyWidgetComponent->SetHiddenInGame(false);
	ReadyWidgetComponent->SetVisibility(true, true);
}

void ALobbyVRCharacter::HideReadyWidget()
{
	if (!ReadyWidgetComponent)
	{
		return;
	}

	ReadyWidgetComponent->SetHiddenInGame(true);
	ReadyWidgetComponent->SetVisibility(false, true);
}
