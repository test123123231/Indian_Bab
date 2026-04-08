// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "MRUtilityKitSubsystem.h"
#include "MRUtilityKitAnchor.h"
#include "Kismet/GameplayStatics.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MRUtilityKitPositionGenerator.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Pawn.h"
#include "OculusXRRoomLayoutManagerComponent.h"
#include "OculusXRSceneEventDelegates.h"
#include "OculusXRSceneFunctionLibrary.h"
#include "Engine/Engine.h"
#include "MRUtilityKitOpenXrExtensionPlugin.h"
#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR
#if PLATFORM_ANDROID
#include "IVulkanDynamicRHI.h"
#endif
#include "OculusXRFunctionLibrary.h"
#include "Generated/MRUtilityKitShared.h"
#include "XRTrackingSystemBase.h"
#include "OculusXRHMDRuntimeSettings.h"
#include "MRUtilityKitSharedHelper.h"
#include "MRUtilityKitTelemetry.h"
#include "OculusXRRoomLayoutManagerComponent.h"

AMRUKRoom* FindRoomByUuid(UMRUKSubsystem* Subsystem, const FOculusXRUUID& RoomUuid)
{
	for (int i = 0; i < Subsystem->Rooms.Num(); ++i)
	{
		if (RoomUuid == Subsystem->Rooms[i]->AnchorUUID)
		{
			return Subsystem->Rooms[i];
		}
	}
	return nullptr;
}

static void UpdateRoomAnchorProperties(const MRUKShared::RoomAnchor* RoomAnchor, AMRUKRoom* Room)
{
	const float WorldToMeters = Room->GetWorld() ? Room->GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;

	Room->SceneModel = ToUnreal(RoomAnchor->sceneModel);
	Room->AnchorUUID = ToUnreal(RoomAnchor->uuid);
	Room->SpaceHandle = RoomAnchor->space;
	Room->SetActorTransform(ToUnreal(RoomAnchor->pose, WorldToMeters));
	Room->RoomMesh = NewObject<UMRUKRoomMesh>(Room);

	const MRUKShared::RoomMesh& RoomMesh = RoomAnchor->roomMesh;

	// Set the vertex buffer of the room mesh
	Room->RoomMesh->Vertices.SetNum(RoomMesh.verticesCount);
	for (uint32_t i = 0; i < RoomMesh.verticesCount; ++i)
	{
		Room->RoomMesh->Vertices[i] = PositionToUnreal(RoomMesh.vertices[i], WorldToMeters);
	}

	// Set the faces of the room mesh
	Room->RoomMesh->Faces.SetNum(RoomMesh.facesCount);
	for (uint32_t i = 0; i < RoomMesh.facesCount; ++i)
	{
		const MRUKShared::RoomFace& face = RoomMesh.faces[i];
		FMRUKRoomFace& RoomFace = Room->RoomMesh->Faces[i];
		RoomFace.Uuid = ToUnreal(face.uuid);
		RoomFace.ParentUuid = ToUnreal(face.parentUuid);
		RoomFace.SemanticClassification = ToUnreal(face.semanticLabel);
		RoomFace.Indices.SetNum(face.indicesCount);
		for (uint32_t j = 0; j < face.indicesCount; ++j)
		{
			RoomFace.Indices[j] = face.indices[j];
		}
	}
}

static void MrukSetTrackingSpacePose(MRUKShared::Posef Pose)
{
	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GEngine, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			const float WorldToMeters = Pawn->GetWorld() ? Pawn->GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;
			auto Transform = ToUnreal(Pose, WorldToMeters);
			Pawn->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation());
		}
	}
}

static MRUKShared::Posef MrukGetTrackingSpacePose()
{
	const FXRTrackingSystemBase* TS = static_cast<FXRTrackingSystemBase*>(GEngine->XRSystem.Get());
	const auto TrackingToWorld = TS->GetTrackingToWorldTransform();
	return ToMrukShared(TrackingToWorld, TS->GetWorldToMetersScale());
}

static void MrukOnPreRoomAnchorAdded(const MRUKShared::RoomAnchor* RoomAnchor, void* UserContext)
{
	auto* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = Subsystem->SpawnRoom();
	UpdateRoomAnchorProperties(RoomAnchor, Room);
}

static void MrukOnRoomAnchorAdded(const MRUKShared::RoomAnchor* RoomAnchor, void* UserContext)
{
	auto* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(RoomAnchor->uuid));
	// Room has been added before in MrukOnPreRoomAnchorAdded()
	check(Room);
	Room->InitializeRoom();
	Subsystem->OnRoomCreated.Broadcast(Room);
}

static void MrukOnRoomAnchorUpdated(const MRUKShared::RoomAnchor* RoomAnchor, const MRUKShared::Uuid* OldRoomAnchorUuid, bool SignificantChange, void* UserContext)
{
	UMRUKSubsystem* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(*OldRoomAnchorUuid));
	check(Room);
	UpdateRoomAnchorProperties(RoomAnchor, Room);
	if (SignificantChange)
	{
		Room->InitializeRoom();
		Subsystem->OnRoomUpdated.Broadcast(Room);
	}
}

static void MrukOnRoomAnchorRemoved(const MRUKShared::RoomAnchor* RoomAnchor, void* UserContext)
{
	UMRUKSubsystem* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(RoomAnchor->uuid));
	Subsystem->OnRoomRemoved.Broadcast(Room);
	Subsystem->Rooms.Remove(Room);
	Room->Destroy();
}

static void UpdateAnchorProperties(const MRUKShared::SceneAnchor* SceneAnchor, AMRUKRoom* Room, AMRUKAnchor* Anchor)
{
	FBox2D Plane;
	TArray<FVector2D> PlaneBoundary;
	if (SceneAnchor->hasPlane)
	{
		Plane = ToUnreal(Room->GetWorld(), SceneAnchor->plane);
		PlaneBoundary = ToUnreal(Room->GetWorld(), SceneAnchor->planeBoundary, SceneAnchor->planeBoundaryCount);
	}
	else
	{
		Plane.bIsValid = false;
	}

	FBox3d Volume;
	if (SceneAnchor->hasVolume)
	{
		Volume = ToUnreal(Room->GetWorld(), SceneAnchor->volume);
	}
	else
	{
		Volume.IsValid = false;
	}

	TArray<FVector> GlobalMeshPositions;
	TArray<int> GlobalMeshIndices;
	if (SceneAnchor->globalMeshPositionsCount > 0 && SceneAnchor->globalMeshIndicesCount > 0)
	{
		GlobalMeshIndices.SetNum(SceneAnchor->globalMeshIndicesCount);
		memcpy(GlobalMeshIndices.GetData(), SceneAnchor->globalMeshIndices, GlobalMeshIndices.Num() * sizeof(int));

		GlobalMeshPositions.SetNum(SceneAnchor->globalMeshPositionsCount);
		for (uint32 i = 0; i < SceneAnchor->globalMeshPositionsCount; ++i)
		{
			const FVector3f& P = SceneAnchor->globalMeshPositions[i];
			GlobalMeshPositions[i] = FVector(-P.Z, P.X, P.Y);
		}
	}

	const float WorldToMeters = Room->GetWorld() ? Room->GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;

	Anchor->Setup(ToUnreal(SceneAnchor->uuid), SceneAnchor->space, ToUnreal(SceneAnchor->pose, WorldToMeters),
		TArray{ ToUnreal(SceneAnchor->semanticLabel) }, Plane,
		PlaneBoundary, Volume, std::move(GlobalMeshPositions), std::move(GlobalMeshIndices));
}

static void MrukOnSceneAnchorAdded(const MRUKShared::SceneAnchor* SceneAnchor, void* UserContext)
{
	UMRUKSubsystem* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(SceneAnchor->roomUuid));
	check(Room);
	AMRUKAnchor* Anchor = Room->SpawnAnchor();
	UpdateAnchorProperties(SceneAnchor, Room, Anchor);
	Room->AddAnchorToRoom(Anchor);
	Room->OnAnchorCreated.Broadcast(Anchor);
}

static void MrukOnSceneAnchorUpdated(const MRUKShared::SceneAnchor* SceneAnchor, bool SignificantChange, void* UserContext)
{
	UMRUKSubsystem* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(SceneAnchor->roomUuid));
	check(Room);
	AMRUKAnchor* Anchor = Room->FindAnchorByUuid(ToUnreal(SceneAnchor->uuid));
	check(Anchor);
	UpdateAnchorProperties(SceneAnchor, Room, Anchor);
	if (SignificantChange)
	{
		Room->OnAnchorUpdated.Broadcast(Anchor);
	}
}

static void MrukOnSceneAnchorRemoved(const MRUKShared::SceneAnchor* SceneAnchor, void* UserContext)
{
	UMRUKSubsystem* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	AMRUKRoom* Room = FindRoomByUuid(Subsystem, ToUnreal(SceneAnchor->roomUuid));
	check(Room);
	AMRUKAnchor* Anchor = Room->FindAnchorByUuid(ToUnreal(SceneAnchor->uuid));
	Room->OnAnchorRemoved.Broadcast(Anchor);
	Room->RemoveAnchor(Anchor);
}

static void MrukOnDiscoveryFinished(MRUKShared::Result Result, void* UserContext)
{
	auto* Subsystem = static_cast<UMRUKSubsystem*>(UserContext);
	const bool Success = Result == MRUKShared::Result::Success;
	if (Success)
	{
		Subsystem->SceneLoadStatus = EMRUKInitStatus::Complete;
	}
	else
	{
		Subsystem->SceneLoadStatus = EMRUKInitStatus::Failed;
	}

	bool HiFiSceneUsed = false;
	for (AMRUKRoom* Room : Subsystem->Rooms)
	{
		if (Room->SceneModel == EMRUKSceneModel::V2 || Room->SceneModel == EMRUKSceneModel::V2FallbackV1)
		{
			// V2_Fallback_V1 isn't a valid value for scene model. Handle it anyway for robustness.
			HiFiSceneUsed = true;
			break;
		}
	}

	if (Subsystem->Rooms.Num() > 0 && Subsystem->Rooms[Subsystem->Rooms.Num() - 1]->SpaceHandle != FOculusXRUInt64{})
	{
		OculusXRTelemetry::TScopedMarker<MRUKTelemetry::FLoadSceneFromDeviceMarker> Event(static_cast<int>(GetTypeHash(Subsystem)));
		Event.AddAnnotation("NumRooms", TCHAR_TO_ANSI(*FString::FromInt(Subsystem->Rooms.Num())));
		Event.AddAnnotation("HiFiScene", HiFiSceneUsed ? "true" : "false");
		Event.SetResult(Subsystem->Rooms.Num() > 0 ? OculusXRTelemetry::EAction::Success : OculusXRTelemetry::EAction::Fail);
	}
	else
	{
		OculusXRTelemetry::TScopedMarker<MRUKTelemetry::FLoadSceneFromJsonMarker> Event(static_cast<int>(GetTypeHash(Subsystem)));
		Event.AddAnnotation("NumRooms", TCHAR_TO_ANSI(*FString::FromInt(Subsystem->Rooms.Num())));
		Event.AddAnnotation("HiFiScene", HiFiSceneUsed ? "true" : "false");
		Event.SetResult(Subsystem->Rooms.Num() > 0 ? OculusXRTelemetry::EAction::Success : OculusXRTelemetry::EAction::Fail);
	}

	Subsystem->OnSceneLoaded.Broadcast(Success);
}

AMRUKAnchor* UMRUKSubsystem::Raycast(const FVector& Origin, const FVector& Direction, float MaxDist, const FMRUKLabelFilter& LabelFilter, FMRUKHit& OutHit)
{
	AMRUKAnchor* HitComponent = nullptr;
	for (const auto& Room : Rooms)
	{
		FMRUKHit HitResult;
		if (!Room)
		{
			continue;
		}
		if (AMRUKAnchor* Anchor = Room->Raycast(Origin, Direction, MaxDist, LabelFilter, HitResult))
		{
			// Prevent further hits which are further away from being found
			MaxDist = HitResult.HitDistance;
			OutHit = HitResult;
			HitComponent = Anchor;
		}
	}
	return HitComponent;
}

bool UMRUKSubsystem::RaycastAll(const FVector& Origin, const FVector& Direction, float MaxDist, const FMRUKLabelFilter& LabelFilter, TArray<FMRUKHit>& OutHits, TArray<AMRUKAnchor*>& OutAnchors)
{
	bool HitAnything = false;
	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (Room->RaycastAll(Origin, Direction, MaxDist, LabelFilter, OutHits, OutAnchors))
		{
			HitAnything = true;
		}
	}
	return HitAnything;
}

static void OpenXrEventHandler(void* Data, void* Context)
{
	MRUKShared::GetInstance()->OnOpenXrEvent(Data);
}

static bool IsOpenXRSystem()
{
	const FName SystemName(TEXT("OpenXR"));
	return GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName);
}

static void SharedLibraryLogPrinter(MRUKShared::LogLevel Level, const char* Message, uint32_t Length)
{
	switch (Level)
	{
		case MRUKShared::LogLevel::Debug:
		case MRUKShared::LogLevel::Info:
			UE_LOG(LogMRUK, Log, TEXT("MRUK Shared: %.*hs"), Length, Message);
			break;
		case MRUKShared::LogLevel::Warn:
			UE_LOG(LogMRUK, Warning, TEXT("MRUK Shared: %.*hs"), Length, Message);
			break;
		case MRUKShared::LogLevel::Error:
			UE_LOG(LogMRUK, Error, TEXT("MRUK Shared: %.*hs"), Length, Message);
			break;
	}
}

void UMRUKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();
	EnableWorldLock = Settings->EnableWorldLock;

	FMRUKModule& MRUKModule = FMRUKModule::GetInstance();
	if (IsOpenXRSystem())
	{
		MRUKModule.OpenXrExtension->OpenXrEventHandler = OpenXrEventHandler;
		MRUKModule.OpenXrExtension->Context = this;
	}

	MRUKShared::LoadMRUKSharedLibrary();

	MRUKShared::GetInstance()->SetLogPrinter(SharedLibraryLogPrinter);

	MRUKShared::Config MrukConfig{};
	MrukConfig.isLinearColorSpace = true;
	MrukConfig.useScenelessWorldLocking = Settings->bUseScenelessWorldLocking;
	MRUKShared::Result result = MRUKShared::GetInstance()->CreateGlobalContext(&MrukConfig);
	if (result != MRUKShared::Result::Success)
	{
		UE_LOG(LogMRUK, Error, TEXT("Failed to initialize global context. It will not be possible to load anchors: %d"), result);
	}
	else
	{
		UE_LOG(LogMRUK, Log, TEXT("Global context initialized successfully"));
	}

#if PLATFORM_ANDROID
	if (GetIVulkanDynamicRHI())
	{
		MRUKShared::GetInstance()->InitGraphics(MRUKShared::RendererType::Vulkan);
	}
#endif

	uint64_t OpenXrInstance = 0;
	uint64_t OpenXrSession = 0;
	UOculusXRFunctionLibrary::GetNativeOpenXRHandles(&OpenXrInstance, &OpenXrSession);
	if (OpenXrInstance == 0 || OpenXrSession == 0)
	{
		// We are probably running in unit tests
		EnableOpenXr = false;
	}
	else
	{
		EnableOpenXr = true;
	}

	if (EnableOpenXr)
	{
		void* OpenXrInstanceProcAddr = nullptr;
		UOculusXRFunctionLibrary::GetOpenXRInstanceProcAddrFunc(&OpenXrInstanceProcAddr);
		ensureMsgf(OpenXrInstanceProcAddr != nullptr, TEXT("OpenXrInstanceProcAddr is not set"));

		UOculusXRFunctionLibrary::GetAppSpace(&OpenXrBaseSpace);
		ensureMsgf(OpenXrBaseSpace != 0, TEXT("BaseSpace not set"));
		if (!IsOpenXRSystem())
		{
			UOculusXRFunctionLibrary::RegisterOpenXrEventHandler(OpenXrEventHandler, this);
		}

		result = MRUKShared::GetInstance()->InitOpenXr(OpenXrInstance, OpenXrSession, OpenXrInstanceProcAddr, OpenXrBaseSpace, NULL, 0);
		if (result != MRUKShared::Result::Success)
		{
			UE_LOG(LogMRUK, Error, TEXT("Failed to initialize Open XR. It will not be possible to load anchors: %d"), result);
		}
		else
		{
			UE_LOG(LogMRUK, Log, TEXT("Open XR initialized successfully"));
		}
	}

	MRUKShared::EventListener EventListener{
		.onPreRoomAnchorAdded = MrukOnPreRoomAnchorAdded,
		.onRoomAnchorAdded = MrukOnRoomAnchorAdded,
		.onRoomAnchorUpdated = MrukOnRoomAnchorUpdated,
		.onRoomAnchorRemoved = MrukOnRoomAnchorRemoved,
		.onSceneAnchorAdded = MrukOnSceneAnchorAdded,
		.onSceneAnchorUpdated = MrukOnSceneAnchorUpdated,
		.onSceneAnchorRemoved = MrukOnSceneAnchorRemoved,
		.onDiscoveryFinished = MrukOnDiscoveryFinished,
		.userContext = this,
	};
	MRUKShared::GetInstance()->RegisterEventListener(EventListener);
	MRUKShared::GetInstance()->SetTrackingSpacePoseGetter(MrukGetTrackingSpacePose);
	MRUKShared::GetInstance()->SetTrackingSpacePoseSetter(MrukSetTrackingSpacePose);
}

void UMRUKSubsystem::Deinitialize()
{
	if (EnableOpenXr)
	{
		UOculusXRFunctionLibrary::UnregisterOpenXrEventHandler(OpenXrEventHandler);
	}

	if (MRUKShared::GetInstance())
	{
		MRUKShared::GetInstance()->DestroyGlobalContext();
	}

	MRUKShared::FreeMRUKSharedLibrary();

	FMRUKModule& MRUKModule = FMRUKModule::GetInstance();
	MRUKModule.OpenXrExtension->OpenXrEventHandler = nullptr;
	MRUKModule.OpenXrExtension->Context = nullptr;
}

void UMRUKSubsystem::UnregisterRoom(AMRUKRoom* Room)
{
	bool Found = false;
	for (const AMRUKRoom* R : Rooms)
	{
		if (Room->AnchorUUID == R->AnchorUUID)
		{
			Found = true;
			break;
		}
	}
	if (Found)
	{
		MRUKShared::Uuid Uuid{};
		memcpy(&Uuid.data, Room->AnchorUUID.UUIDBytes, 2 * sizeof(uint64_t));
		MRUKShared::GetInstance()->ClearRoom(Uuid);
	}
}

AMRUKRoom* UMRUKSubsystem::GetCurrentRoom() const
{
	// This is a rather expensive operation, we should only do it at most once per frame.
	if (CachedCurrentRoomFrame != GFrameCounter)
	{
		if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (APawn* Pawn = PlayerController->GetPawn())
			{
				const auto& PawnTransform = Pawn->GetActorTransform();

				FVector HeadPosition;
				FRotator Unused;

				// Get the position and rotation of the VR headset
				UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(Unused, HeadPosition);

				HeadPosition = PawnTransform.TransformPosition(HeadPosition);

				for (const auto& Room : Rooms)
				{
					if (IsValid(Room) && Room->IsPositionInRoom(HeadPosition))
					{
						CachedCurrentRoom = Room;
						CachedCurrentRoomFrame = GFrameCounter;
						return Room;
					}
				}
			}
		}
	}

	if (IsValid(CachedCurrentRoom))
	{
		return CachedCurrentRoom;
	}

	for (const auto& Room : Rooms)
	{
		if (IsValid(Room))
		{
			return Room;
		}
	}
	return nullptr;
}

FString UMRUKSubsystem::SaveSceneToJsonString()
{
	const char* Json = MRUKShared::GetInstance()->SaveSceneToJson(true, NULL, 0);
	const FString Result(Json);
	MRUKShared::GetInstance()->FreeJson(Json);
	return Result;
}

void UMRUKSubsystem::LoadSceneFromJsonString(const FString& String, EMRUKSceneModel SceneModel)
{
	if (SceneLoadStatus == EMRUKInitStatus::Busy)
	{
		UE_LOG(LogMRUK, Error, TEXT("Can't start loading a scene from JSON while the scene is already loading"));
		return;
	}

	if (SceneModel == EMRUKSceneModel::V2 || SceneModel == EMRUKSceneModel::V2FallbackV1)
	{
		OculusXRTelemetry::TScopedMarker<MRUKTelemetry::FLoadHiFiScene> Event(static_cast<int>(GetTypeHash(this)));
	}

	const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();
	SceneLoadStatus = EMRUKInitStatus::Busy;
	MRUKShared::Result result = MRUKShared::GetInstance()->LoadSceneFromJson(TCHAR_TO_ANSI(*String), true, ToMrukShared(SceneModel));
	if (result != MRUKShared::Result::Success)
	{
		SceneLoadStatus = EMRUKInitStatus::Failed;
		MrukOnDiscoveryFinished(result, this);
	}
}

void UMRUKSubsystem::LoadSceneFromDevice(EMRUKSceneModel SceneModel)
{
	if (SceneLoadStatus == EMRUKInitStatus::Busy)
	{
		UE_LOG(LogMRUK, Error, TEXT("Can't start loading from device while the scene is already loading"));
		return;
	}

	if (SceneModel == EMRUKSceneModel::V2 || SceneModel == EMRUKSceneModel::V2FallbackV1)
	{
		OculusXRTelemetry::TScopedMarker<MRUKTelemetry::FLoadHiFiScene> Event(static_cast<int>(GetTypeHash(this)));
	}

	SceneLoadStatus = EMRUKInitStatus::Busy;
	const UOculusXRHMDRuntimeSettings* Settings = GetMutableDefault<UOculusXRHMDRuntimeSettings>();
	MRUKShared::Result result = MRUKShared::GetInstance()->StartDiscovery(true, ToMrukShared(SceneModel));
	if (result != MRUKShared::Result::Success)
	{
		SceneLoadStatus = EMRUKInitStatus::Failed;
		UE_LOG(LogMRUK, Error, TEXT("Failed to start anchor discovery: %d"), result);
		MrukOnDiscoveryFinished(result, this);
	}
}

void UMRUKSubsystem::ClearScene()
{
	SceneLoadStatus = EMRUKInitStatus::None;
	MRUKShared::GetInstance()->ClearRooms();
}

AMRUKAnchor* UMRUKSubsystem::TryGetClosestSurfacePosition(const FVector& WorldPosition, FVector& OutSurfacePosition, const FMRUKLabelFilter& LabelFilter, double MaxDistance)
{
	AMRUKAnchor* ClosestAnchor = nullptr;

	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}
		double SurfaceDistance{};
		FVector SurfacePos{};
		if (const auto Anchor = Room->TryGetClosestSurfacePosition(WorldPosition, SurfacePos, SurfaceDistance, LabelFilter, MaxDistance))
		{
			ClosestAnchor = Anchor;
			OutSurfacePosition = SurfacePos;
			MaxDistance = SurfaceDistance;
		}
	}

	return ClosestAnchor;
}

AMRUKAnchor* UMRUKSubsystem::TryGetClosestSeatPose(const FVector& RayOrigin, const FVector& RayDirection, FTransform& OutSeatTransform)
{
	AMRUKAnchor* ClosestAnchor = nullptr;
	double ClosestSeatDistanceSq = DBL_MAX;

	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}

		FTransform SeatTransform{};
		if (AMRUKAnchor* Anchor = Room->TryGetClosestSeatPose(RayOrigin, RayDirection, SeatTransform))
		{
			const double SeatDistanceSq = (RayOrigin - Anchor->GetActorTransform().GetTranslation()).SquaredLength();
			if (SeatDistanceSq < ClosestSeatDistanceSq)
			{
				ClosestAnchor = Anchor;
				ClosestSeatDistanceSq = SeatDistanceSq;
				OutSeatTransform = SeatTransform;
			}
		}
	}

	return ClosestAnchor;
}

AMRUKAnchor* UMRUKSubsystem::GetBestPoseFromRaycast(const FVector& RayOrigin, const FVector& RayDirection, double MaxDist, const FMRUKLabelFilter& LabelFilter, FTransform& OutPose, EMRUKPositioningMethod PositioningMethod)
{
	AMRUKAnchor* ClosestAnchor = nullptr;
	double ClosestPoseDistanceSq = DBL_MAX;

	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}

		FTransform Pose{};
		AMRUKAnchor* Anchor = Room->GetBestPoseFromRaycast(RayOrigin, RayDirection, MaxDist, LabelFilter, Pose, PositioningMethod);
		if (Anchor)
		{
			const double PoseDistanceSq = (RayOrigin - OutPose.GetTranslation()).SquaredLength();
			if (PoseDistanceSq < ClosestPoseDistanceSq)
			{
				ClosestAnchor = Anchor;
				ClosestPoseDistanceSq = PoseDistanceSq;
				OutPose = Pose;
			}
		}
	}

	return ClosestAnchor;
}

AMRUKAnchor* UMRUKSubsystem::GetKeyWall(double Tolerance)
{
	if (AMRUKRoom* CurrentRoom = GetCurrentRoom())
	{
		return CurrentRoom->GetKeyWall(Tolerance);
	}
	return nullptr;
}

AMRUKAnchor* UMRUKSubsystem::GetLargestSurface(const FString& Label)
{
	if (AMRUKRoom* CurrentRoom = GetCurrentRoom())
	{
		return CurrentRoom->GetLargestSurface(Label);
	}
	return nullptr;
}

AMRUKAnchor* UMRUKSubsystem::IsPositionInSceneVolume(const FVector& WorldPosition, bool TestVerticalBounds, double Tolerance)
{
	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (const auto Anchor = Room->IsPositionInSceneVolume(WorldPosition, TestVerticalBounds, Tolerance))
		{
			return Anchor;
		}
	}
	return nullptr;
}

TArray<AActor*> UMRUKSubsystem::SpawnInterior(const TMap<FString, FMRUKSpawnGroup>& SpawnGroups, const TArray<FString>& CutHoleLabels, UMaterialInterface* ProceduralMaterial, bool ShouldFallbackToProcedural)
{
	return SpawnInteriorFromStream(SpawnGroups, FRandomStream(NAME_None), CutHoleLabels, ProceduralMaterial, ShouldFallbackToProcedural);
}

TArray<AActor*> UMRUKSubsystem::SpawnInteriorFromStream(const TMap<FString, FMRUKSpawnGroup>& SpawnGroups, const FRandomStream& RandomStream, const TArray<FString>& CutHoleLabels, UMaterialInterface* ProceduralMaterial, bool ShouldFallbackToProcedural)
{
	TArray<AActor*> AllInteriorActors;

	for (const auto& Room : Rooms)
	{
		if (!Room)
		{
			continue;
		}
		auto InteriorActors = Room->SpawnInteriorFromStream(SpawnGroups, RandomStream, CutHoleLabels, ProceduralMaterial, ShouldFallbackToProcedural);
		AllInteriorActors.Append(InteriorActors);
	}

	return AllInteriorActors;
}

bool UMRUKSubsystem::LaunchSceneCapture()
{
	const bool Success = GetRoomLayoutManager()->LaunchCaptureFlow();
	if (Success)
	{
		UE_LOG(LogMRUK, Log, TEXT("Capture flow launched with success"));
	}
	else
	{
		UE_LOG(LogMRUK, Error, TEXT("Launching capture flow failed!"));
	}
	return Success;
}

void UMRUKSubsystem::CreateEnvironmentRaycaster()
{
	MRUKShared::Result result = MRUKShared::GetInstance()->CreateEnvironmentRaycaster();
	if (result != MRUKShared::Result::Success)
	{
		UE_LOG(LogMRUK, Error, TEXT("Failed to create environment raycaster: %d"), result);
	}
}

void UMRUKSubsystem::DestroyEnvironmentRaycaster()
{
	MRUKShared::GetInstance()->DestroyEnvironmentRaycaster();
}

EMRUKEnvironmentRaycasterStatus UMRUKSubsystem::EnvironmentRaycasterStatus() const
{
	switch (MRUKShared::GetInstance()->EnvironmentRaycasterStatus())
	{
		case MRUKShared::EnvironmentRaycasterStatus::Creating:
			return EMRUKEnvironmentRaycasterStatus::Creating;
		case MRUKShared::EnvironmentRaycasterStatus::Stopped:
			return EMRUKEnvironmentRaycasterStatus::Stopped;
		case MRUKShared::EnvironmentRaycasterStatus::Ready:
			return EMRUKEnvironmentRaycasterStatus::Ready;
	}

	return EMRUKEnvironmentRaycasterStatus::Stopped;
}

FMRUKEnvironmentRaycastHit UMRUKSubsystem::RaycastEnvironment(const FVector& Origin, const FVector& Direction, float MaxDistance)
{
	const FXRTrackingSystemBase* TS = static_cast<FXRTrackingSystemBase*>(GEngine->XRSystem.Get());
	const auto TrackingToWorld = TS->GetTrackingToWorldTransform();
	const auto WorldToTracking = TrackingToWorld.Inverse();

	const FVector DirectionNormalized = Direction.GetSafeNormal();
	const float WorldToMeters = GetWorld()->GetWorldSettings()->WorldToMeters;

	if (MaxDistance == 0.0f)
	{
		MaxDistance = FLT_MAX;
	}
	else
	{
		MaxDistance /= WorldToMeters;
	}

	MRUKShared::EnvironmentRaycastHitPointGetInfo info{};
	info.startPoint = PositionToMrukShared(WorldToTracking.TransformPosition(Origin), WorldToMeters);
	info.direction = UnitVectorToMrukShared(WorldToTracking.TransformVector(DirectionNormalized));
	info.maxDistance = MaxDistance;

	MRUKShared::EnvironmentRaycastHitPoint hitPoint{};
	MRUKShared::Result result = MRUKShared::GetInstance()->RaycastEnvironment(&info, &hitPoint);

	FMRUKEnvironmentRaycastHit raycastHit{};
	raycastHit.point = TrackingToWorld.TransformPosition(PositionToUnreal(hitPoint.point, WorldToMeters));
	raycastHit.normal = TrackingToWorld.TransformVector(-UnitVectorToUnreal(hitPoint.normal)).GetSafeNormal();
	raycastHit.orientation = TrackingToWorld.TransformRotation(ToUnreal(hitPoint.orientation));

	if (result != MRUKShared::Result::Success)
	{
		raycastHit.status = EMRUKEnvironmentRaycastHitStatus::Failure;
	}
	else
	{
		switch (hitPoint.status)
		{
			case MRUKShared::EnvironmentRaycastStatus::InvalidOrientation:
			case MRUKShared::EnvironmentRaycastStatus::Hit:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::Hit;
				break;

			case MRUKShared::EnvironmentRaycastStatus::NoHit:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::NoHit;
				break;
			case MRUKShared::EnvironmentRaycastStatus::HitPointOccluded:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::HitPointOccluded;
				break;
			case MRUKShared::EnvironmentRaycastStatus::HitPointOutsideFov:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::HitPointOutsideFov;
				break;
			case MRUKShared::EnvironmentRaycastStatus::RayOccluded:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::RayOccluded;
				break;
			case MRUKShared::EnvironmentRaycastStatus::Max:
				raycastHit.status = EMRUKEnvironmentRaycastHitStatus::Failure;
				break;
		}
	}

	return raycastHit;
}

FBox UMRUKSubsystem::GetActorClassBounds(TSubclassOf<AActor> Actor)
{
	if (const auto Entry = ActorClassBoundsCache.Find(Actor))
	{
		return *Entry;
	}
	const auto TempActor = GetWorld()->SpawnActor(Actor);
	const auto Bounds = TempActor->CalculateComponentsBoundingBoxInLocalSpace(true);
	TempActor->Destroy();
	ActorClassBoundsCache.Add(Actor, Bounds);
	return Bounds;
}

void UMRUKSubsystem::SceneCaptureComplete(FOculusXRUInt64 RequestId, bool bSuccess)
{
	UE_LOG(LogMRUK, Log, TEXT("Scene capture complete Success==%d"), bSuccess);
	OnCaptureComplete.Broadcast(bSuccess);
}

UOculusXRRoomLayoutManagerComponent* UMRUKSubsystem::GetRoomLayoutManager()
{
	if (!RoomLayoutManager)
	{
		FActorSpawnParameters Params{};
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = nullptr;
		RoomLayoutManagerActor = GetWorld()->SpawnActor<AActor>(Params);
		RoomLayoutManagerActor->SetRootComponent(NewObject<USceneComponent>(RoomLayoutManagerActor, TEXT("SceneComponent")));

		RoomLayoutManagerActor->AddComponentByClass(UOculusXRRoomLayoutManagerComponent::StaticClass(), false, FTransform::Identity, false);
		RoomLayoutManager = RoomLayoutManagerActor->GetComponentByClass<UOculusXRRoomLayoutManagerComponent>();
		RoomLayoutManager->OculusXRRoomLayoutSceneCaptureComplete.AddDynamic(this, &UMRUKSubsystem::SceneCaptureComplete);
	}
	return RoomLayoutManager;
}

bool UMRUKSubsystem::DiscoveryIsRunning() const
{
	return MRUKShared::GetInstance()->IsDiscoveryRunning();
}

AMRUKRoom* UMRUKSubsystem::SpawnRoom()
{
	FActorSpawnParameters ActorSpawnParams;
	ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMRUKRoom* Room = GetWorld()->SpawnActor<AMRUKRoom>(ActorSpawnParams);

#if WITH_EDITOR
	Room->SetActorLabel(TEXT("ROOM"));
#endif

	Rooms.Push(Room);

	return Room;
}

void UMRUKSubsystem::FinishedLoading(bool Success)
{
	UE_LOG(LogMRUK, Log, TEXT("Finished loading: Success==%d"), Success);
	if (Success)
	{
		SceneLoadStatus = EMRUKInitStatus::Complete;
	}
	else
	{
		SceneLoadStatus = EMRUKInitStatus::Failed;
	}
	OnSceneLoaded.Broadcast(Success);
}

void UMRUKSubsystem::Tick(float DeltaTime)
{
	if (EnableWorldLock)
	{
		if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (APawn* Pawn = PlayerController->GetPawn())
			{
				MRUKShared::Posef SharedLibOffset;
				SharedLibOffset.position = { 0.0f, 0.0f, 0.0f };
				SharedLibOffset.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
				if (MRUKShared::GetInstance()->GetWorldLockOffset(&SharedLibOffset))
				{
					const float WorldToMeters = GetWorld() ? GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;
					auto Transform = ToUnreal(SharedLibOffset, WorldToMeters);
					Pawn->SetActorLocationAndRotation(Transform.GetTranslation(), Transform.GetRotation());
				}
			}
		}
	}

	uint64_t NextPredictedDisplayTime = 0;
	if (EnableOpenXr)
	{
		uint64_t NewBaseSpace = 0;
		UOculusXRFunctionLibrary::GetAppSpace(&NewBaseSpace);
		if (NewBaseSpace != OpenXrBaseSpace)
		{
			OpenXrBaseSpace = NewBaseSpace;
			MRUKShared::GetInstance()->SetBaseSpace(OpenXrBaseSpace);
		}

		UOculusXRFunctionLibrary::GetNextPredictedDisplayTime(&NextPredictedDisplayTime);
	}
	MRUKShared::GetInstance()->TickGlobalContext(NextPredictedDisplayTime);
}

bool UMRUKSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_BeginDestroyed) && IsValidChecked(this);
}

