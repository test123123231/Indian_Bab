#include "Character/LobbyVRCharacter.h"

#include "Actor/SeatActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
#include "PlayerState/MainPlayerState.h"
#include "Widget/PlayerNameWidget.h"
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
	ConfigureWidgetInteraction();
	BindVRPlayerStateDelegates();
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	ConfigurePlayerNameWidget();
	BindVRPlayerStateDelegates();

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

void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	BindVRPlayerStateDelegates();
	ConfigurePlayerNameWidget();
	HideLocalPlayerNameWidget();
}

void ALobbyVRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindVRPlayerStateDelegates();
	ConfigurePlayerNameWidget();
	HideLocalPlayerNameWidget();
}

void ALobbyVRCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	BindVRPlayerStateDelegates();
	ConfigurePlayerNameWidget();
	HideLocalPlayerNameWidget();
}

void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVRNameWidget();
	UpdateVRPointers();
}

void ALobbyVRCharacter::BindVRPlayerStateDelegates()
{
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->OnSteamNicknameChanged.RemoveAll(this);
	PS->OnSteamNicknameChanged.AddUObject(this, &ALobbyVRCharacter::UpdateNameWidget);

	PS->OnCardChanged.RemoveAll(this);
	PS->OnCardChanged.AddUObject(this, &ALobbyVRCharacter::UpdateCardWidget);

	UpdateNameWidget();
	UpdateCardWidget();
}

void ALobbyVRCharacter::UpdateNameWidget()
{
	if (!NameWidgetComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] NameWidgetComponent is null on %s"), *GetName());
		return;
	}

	UpdateVRNameWidget();

	if (IsLocallyControlled())
	{
		return;
	}

	if (!NameWidgetComponent->GetWidgetClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] NameWidgetComponent has no WidgetClass on %s"), *GetName());
		return;
	}

	if (!NameWidgetComponent->GetUserWidgetObject())
	{
		NameWidgetComponent->InitWidget();
	}

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(NameWidgetComponent->GetUserWidgetObject());
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] NameWidgetComponent widget is not UPlayerNameWidget on %s"), *GetName());
		return;
	}

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] MainPlayerState is null on %s"), *GetName());
		return;
	}

	FString DisplayName = PS->GetSteamNickname();
	if (DisplayName.IsEmpty())
	{
		DisplayName = FString::Printf(TEXT("Player %d"), PS->GetPlayerId());
	}

	Widget->SetPlayerName(DisplayName);
}

void ALobbyVRCharacter::UpdateCardWidget()
{
	if (!NameWidgetComponent)
	{
		return;
	}

	if (IsLocallyControlled())
	{
		ApplyVRNameWidgetVisibility();
		return;
	}

	if (!NameWidgetComponent->GetWidgetClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] Card update skipped. NameWidgetComponent has no WidgetClass on %s"), *GetName());
		return;
	}

	if (!NameWidgetComponent->GetUserWidgetObject())
	{
		NameWidgetComponent->InitWidget();
	}

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(NameWidgetComponent->GetUserWidgetObject());
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyVRCharacter] Card widget is not UPlayerNameWidget on %s"), *GetName());
		return;
	}

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS)
	{
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
	ConfigureWidgetInteraction();
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
	ConfigureWidgetInteraction();
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

void ALobbyVRCharacter::ConfigureWidgetInteraction()
{
	if (WidgetInteractionRight)
	{
		WidgetInteractionRight->InteractionDistance = VRPointerMaxDistance;
		WidgetInteractionRight->TraceChannel = ECC_Visibility;
		WidgetInteractionRight->bShowDebug = bShowWidgetInteractionDebug;
		WidgetInteractionRight->bEnableHitTesting = true;
		WidgetInteractionRight->InteractionSource = EWidgetInteractionSource::World;
	}

	if (WidgetInteractionLeft)
	{
		WidgetInteractionLeft->InteractionDistance = VRPointerMaxDistance;
		WidgetInteractionLeft->TraceChannel = ECC_Visibility;
		WidgetInteractionLeft->bShowDebug = bShowWidgetInteractionDebug;
		WidgetInteractionLeft->bEnableHitTesting = true;
		WidgetInteractionLeft->InteractionSource = EWidgetInteractionSource::World;
	}
}

void ALobbyVRCharacter::ConfigurePlayerNameWidget()
{
	UpdateVRNameWidget();
}

void ALobbyVRCharacter::UpdateVRNameWidget()
{
	if (!NameWidgetComponent)
	{
		return;
	}

	if (!ApplyVRNameWidgetVisibility())
	{
		return;
	}

	UpdateVRNameWidgetTransform();
}

bool ALobbyVRCharacter::ApplyVRNameWidgetVisibility()
{
	if (!NameWidgetComponent)
	{
		return false;
	}

	NameWidgetComponent->SetOwnerNoSee(true);
	NameWidgetComponent->SetDrawAtDesiredSize(true);
	NameWidgetComponent->SetWidgetSpace(EWidgetSpace::World);

	if (IsLocallyControlled())
	{
		NameWidgetComponent->SetHiddenInGame(true);
		NameWidgetComponent->SetVisibility(false, true);
		LogPlayerNameWidgetTransform();
		return false;
	}

	NameWidgetComponent->SetHiddenInGame(false);
	NameWidgetComponent->SetVisibility(true, true);
	return true;
}

void ALobbyVRCharacter::UpdateVRNameWidgetTransform()
{
	if (!NameWidgetComponent || IsLocallyControlled())
	{
		return;
	}

	NameWidgetComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	NameWidgetComponent->SetWorldLocation(GetVRNameWidgetTargetLocation());

	FVector LocalCameraLocation;
	if (GetLocalCameraLocation(LocalCameraLocation))
	{
		const FVector ToCamera = LocalCameraLocation - NameWidgetComponent->GetComponentLocation();
		if (!ToCamera.IsNearlyZero())
		{
			NameWidgetComponent->SetWorldRotation(ToCamera.Rotation());
		}
	}

	LogPlayerNameWidgetTransform();
}

FVector ALobbyVRCharacter::GetVRNameWidgetTargetLocation() const
{
	if (ThirdPersonMetaHumanBody)
	{
		const bool bHasHeadSocketOrBone =
			ThirdPersonMetaHumanBody->DoesSocketExist(PlayerNameWidgetHeadSocketName) ||
			ThirdPersonMetaHumanBody->GetBoneIndex(PlayerNameWidgetHeadSocketName) != INDEX_NONE;

		if (bHasHeadSocketOrBone)
		{
			return ThirdPersonMetaHumanBody->GetSocketLocation(PlayerNameWidgetHeadSocketName) + VRNameWidgetWorldOffset;
		}

		const FBox BodyBounds = ThirdPersonMetaHumanBody->Bounds.GetBox();
		return FVector(BodyBounds.GetCenter().X, BodyBounds.GetCenter().Y, BodyBounds.Max.Z) + VRNameWidgetWorldOffset;
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, VRNameWidgetHeightOffset) + VRNameWidgetWorldOffset;
}

bool ALobbyVRCharacter::GetLocalCameraLocation(FVector& OutCameraLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const APlayerController* LocalPC = World->GetFirstPlayerController();
	if (!LocalPC || !LocalPC->PlayerCameraManager)
	{
		return false;
	}

	OutCameraLocation = LocalPC->PlayerCameraManager->GetCameraLocation();
	return true;
}

void ALobbyVRCharacter::LogPlayerNameWidgetTransform() const
{
	if (!bLogPlayerNameWidgetTransform || !NameWidgetComponent)
	{
		return;
	}

	const USceneComponent* AttachParent = NameWidgetComponent->GetAttachParent();
	FVector CameraLocation = FVector::ZeroVector;
	GetLocalCameraLocation(CameraLocation);
	UE_LOG(LogTemp, Warning,
		TEXT("[LobbyVRCharacter] NameWidget Character=%s Local=%s AttachParent=%s RelLoc=%s WorldLoc=%s CameraWorldLoc=%s ActorLoc=%s"),
		*GetName(),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(AttachParent),
		*NameWidgetComponent->GetRelativeLocation().ToString(),
		*NameWidgetComponent->GetComponentLocation().ToString(),
		*CameraLocation.ToString(),
		*GetActorLocation().ToString());
}

void ALobbyVRCharacter::HideLocalPlayerNameWidget()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent || WidgetComponent == ReadyWidgetComponent)
		{
			continue;
		}

		const FString ComponentName = WidgetComponent->GetName();
		const bool bLooksLikeNameWidget =
			WidgetComponent == NameWidgetComponent ||
			ComponentName.Contains(TEXT("Name")) ||
			ComponentName.Contains(TEXT("Nick")) ||
			ComponentName.Contains(TEXT("Player"));

		if (!bLooksLikeNameWidget)
		{
			continue;
		}

		WidgetComponent->SetOwnerNoSee(true);
		WidgetComponent->SetHiddenInGame(true);
		WidgetComponent->SetVisibility(false, true);
		WidgetComponent->SetDrawAtDesiredSize(false);
	}
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
		UE_LOG(LogTemp, Verbose, TEXT("[LobbyVRCharacter] %s pointer hit: %s"), PointerName, *GetNameSafe(HitResult.GetActor()));
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
