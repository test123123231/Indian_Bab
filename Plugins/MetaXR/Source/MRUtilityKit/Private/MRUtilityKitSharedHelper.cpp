// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "MRUtilityKitSharedHelper.h"
#include "GameFramework/WorldSettings.h"

#include <XRTrackingSystemBase.h>

MRUKShared::Uuid ToMrukShared(const FOculusXRUUID& Uuid)
{
	MRUKShared::Uuid MrukUuid;
	memcpy(MrukUuid.data, Uuid.UUIDBytes, sizeof(MrukUuid.data));
	return MrukUuid;
}

MRUKShared::SceneModel ToMrukShared(EMRUKSceneModel SceneModel)
{
	switch (SceneModel)
	{
		case EMRUKSceneModel::V1:
			return MRUKShared::SceneModel::V1;
		case EMRUKSceneModel::V2:
			return MRUKShared::SceneModel::V2;
		case EMRUKSceneModel::V2FallbackV1:
			return MRUKShared::SceneModel::V2FallbackV1;
		default:
			return MRUKShared::SceneModel::V1;
	}
}

EMRUKSceneModel ToUnreal(MRUKShared::SceneModel SceneModel)
{
	switch (SceneModel)
	{
		case MRUKShared::SceneModel::V1:
			return EMRUKSceneModel::V1;
		case MRUKShared::SceneModel::V2:
			return EMRUKSceneModel::V2;
		case MRUKShared::SceneModel::V2FallbackV1:
			return EMRUKSceneModel::V2FallbackV1;
		default:
			return EMRUKSceneModel::V1;
	}
}

FOculusXRUUID ToUnreal(const MRUKShared::Uuid& Uuid)
{
	FOculusXRUUID ConvertedUuid;
	memcpy(ConvertedUuid.UUIDBytes, Uuid.data, sizeof(ConvertedUuid.UUIDBytes));
	return ConvertedUuid;
}

FBox3d ToUnreal(const UWorld* World, const MRUKShared::Volume& Volume)
{
	FVector Min{ -Volume.min.Z, Volume.min.X, Volume.min.Y };
	Min.X -= Volume.max.Z - Volume.min.Z;
	const FVector Size{ Volume.max.Z - Volume.min.Z, Volume.max.X - Volume.min.X, Volume.max.Y - Volume.min.Y };
	const FVector Max = Min + Size;

	const float WorldToMeters = World ? World->GetWorldSettings()->WorldToMeters : 100.0f;
	const FBox3d Box(Min * WorldToMeters, Max * WorldToMeters);
	return Box;
}

FBox2d ToUnreal(const UWorld* World, const MRUKShared::Plane& Plane)
{
	const float WorldToMeters = World ? World->GetWorldSettings()->WorldToMeters : 100.0f;
	FBox2d Box(FVector2D{ Plane.x, Plane.y } * WorldToMeters, FVector2D{ Plane.x + Plane.width, Plane.y + Plane.height } * WorldToMeters);
	return Box;
}

TArray<FVector2D> ToUnreal(const UWorld* World, const FVector2f* const Boundary, uint32_t BoundaryCount)
{
	const float WorldToMeters = World ? World->GetWorldSettings()->WorldToMeters : 100.0f;
	TArray<FVector2D> ConvertedBoundary;
	ConvertedBoundary.SetNum(BoundaryCount);
	for (uint32_t i = 0; i < BoundaryCount; ++i)
	{
		ConvertedBoundary[i] = FVector2D{ Boundary[i] } * WorldToMeters;
	}
	return ConvertedBoundary;
}

FString ToUnreal(MRUKShared::Label Label)
{
	switch (Label)
	{
		case MRUKShared::Label::Floor:
			return "FLOOR";
		case MRUKShared::Label::Ceiling:
			return "CEILING";
		case MRUKShared::Label::WallFace:
			return "WALL_FACE";
		case MRUKShared::Label::Table:
			return "TABLE";
		case MRUKShared::Label::Couch:
			return "COUCH";
		case MRUKShared::Label::DoorFrame:
			return "DOOR_FRAME";
		case MRUKShared::Label::WindowFrame:
			return "WINDOW_FRAME";
		case MRUKShared::Label::Other:
			return "OTHER";
		case MRUKShared::Label::Storage:
			return "STORAGE";
		case MRUKShared::Label::Bed:
			return "BED";
		case MRUKShared::Label::Screen:
			return "SCREEN";
		case MRUKShared::Label::Lamp:
			return "LAMP";
		case MRUKShared::Label::Plant:
			return "PLANT";
		case MRUKShared::Label::WallArt:
			return "WALL_ART";
		case MRUKShared::Label::SceneMesh:
			return "GLOBAL_MESH";
		case MRUKShared::Label::InvisibleWallFace:
			return "INVISIBLE_WALL_FACE";
		case MRUKShared::Label::Unknown:
			return "UNKNOWN";
		case MRUKShared::Label::InnerWallFace:
			return "INNER_WALL_FACE";
		case MRUKShared::Label::Tabletop:
			return "TABLETOP";
		case MRUKShared::Label::SittingArea:
			return "SITTING_AREA";
		case MRUKShared::Label::SleepingArea:
			return "SLEEPING_AREA";
		case MRUKShared::Label::StorageTop:
			return "STORAGE_TOP";
		default:
			return "UNKNOWN";
	}
}

uint32_t ToMrukSharedSurfaceTypes(int32 ComponentTypes)
{
	uint32_t SurfaceTypes = 0;

	if ((ComponentTypes & (int32)EMRUKComponentType::Plane) != 0)
	{
		SurfaceTypes |= (uint32_t)MRUKShared::SurfaceType::Plane;
	}
	if ((ComponentTypes & (int32)EMRUKComponentType::Volume) != 0)
	{
		SurfaceTypes |= (uint32_t)MRUKShared::SurfaceType::Volume;
	}
	if ((ComponentTypes & (int32)EMRUKComponentType::Mesh) != 0)
	{
		SurfaceTypes |= (uint32_t)MRUKShared::SurfaceType::Mesh;
	}

	return SurfaceTypes;
}

MRUKShared::LabelFilter ToMrukShared(const FMRUKLabelFilter LabelFilter)
{
	MRUKShared::LabelFilter Filter{};

	for (int i = 0; i < LabelFilter.IncludedLabels.Num(); ++i)
	{
		Filter.includedLabelsSet = true;
		Filter.includedLabels |= (uint32_t)MRUKShared::GetInstance()->StringToMrukLabel(TCHAR_TO_ANSI(*LabelFilter.IncludedLabels[i]));
	}

	for (int i = 0; i < LabelFilter.ExcludedLabels.Num(); ++i)
	{
		Filter.includedLabelsSet = true;
		Filter.includedLabels &= ~(uint32_t)MRUKShared::GetInstance()->StringToMrukLabel(TCHAR_TO_ANSI(*LabelFilter.ExcludedLabels[i]));
	}

	Filter.surfaceType = ToMrukSharedSurfaceTypes(LabelFilter.ComponentTypes);

	return Filter;
}

FVector UnitVectorToUnreal(const FVector3f& UnitVector)
{
	return FVector(-UnitVector.Z, UnitVector.X, UnitVector.Y);
}

FVector3f UnitVectorToMrukShared(const FVector& UnitVector)
{
	return FVector3f(UnitVector.Y, UnitVector.Z, -UnitVector.X);
}

FVector PositionToUnreal(const FVector3f& Position, float WorldToMeters)
{
	return UnitVectorToUnreal(Position) * WorldToMeters;
}

FVector3f PositionToMrukShared(const FVector& Position, float WorldToMeters)
{
	return UnitVectorToMrukShared(Position) / WorldToMeters;
}

FQuat ToUnreal(const MRUKShared::Quatf& Q)
{
	return FQuat(-Q.z, Q.x, Q.y, -Q.w);
}

MRUKShared::Quatf ToMrukShared(const FQuat& Q)
{
	return MRUKShared::Quatf{ static_cast<float>(Q.Y), static_cast<float>(Q.Z), -static_cast<float>(Q.X), -static_cast<float>(Q.W) };
}

FTransform ToUnreal(const MRUKShared::Posef& Pose, float WorldToMeters)
{
	return FTransform(ToUnreal(Pose.rotation), PositionToUnreal(Pose.position, WorldToMeters));
}

MRUKShared::Posef ToMrukShared(const FTransform& Transform, float WorldToMeters)
{
	return MRUKShared::Posef{ PositionToMrukShared(Transform.GetTranslation(), WorldToMeters), ToMrukShared(Transform.GetRotation()) };
}
