// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "MRUtilityKitSceneDataProvider.h"
#include "UObject/ConstructorHelpers.h"
#include "MRUtilityKit.h"

void AMRUKSceneDataProvider::GetRoom(FString& RoomJSON, FString& RoomName)
{
	if (!bUseRandomRoom)
	{
		if (!SpecificRoomName.IsEmpty())
		{
			for (const TPair<FString, UDataTable*>& Room : Rooms)
			{
				UDataTable* const RoomDT = Room.Value;
				const FJSONData* TmpJSON = RoomDT->FindRow<FJSONData>(FName(SpecificRoomName), "", false);
				if (TmpJSON != nullptr)
				{
					RoomJSON = TmpJSON->JSON;
					RoomName = SpecificRoomName;
					return;
				}
			}
			UE_LOG(LogMRUK, Warning, TEXT("Specific room name not found, using random room."));
		}
		else
		{
			UE_LOG(LogMRUK, Warning, TEXT("Specific room name not defined, using random room."));
		}
	}

	if (bUseRandomRoomFromClass)
	{
		if (!SpecificRoomClass.IsEmpty())
		{
			UDataTable* const RoomDT = *Rooms.Find(SpecificRoomClass);
			if (RoomDT != nullptr)
			{
				TArray<FJSONData*> TmpArray;
				RoomDT->GetAllRows("", TmpArray);
				TArray<FName> TmpRowNames = RoomDT->GetRowNames();
				const int32 NumRows = TmpArray.Num() - 1;
				const int32 RowIndex = FMath::RandRange(0, NumRows);

				RoomJSON = TmpArray[RowIndex]->JSON;
				RoomName = TmpRowNames[RowIndex].ToString();
				return;
			}

			UE_LOG(LogMRUK, Warning, TEXT("Specific room class not found, using random room."));
		}
		else
		{
			UE_LOG(LogMRUK, Warning, TEXT("Specific room class not defined, using random room."));
		}
	}

	const int32 NumRooms = Rooms.Num() - 1;
	const int32 RoomIndex = FMath::RandRange(0, NumRooms);

	TArray<UDataTable*> ChildArray;
	Rooms.GenerateValueArray(ChildArray);

	UDataTable* const Room = ChildArray[RoomIndex];

	const int32 NumRows = Room->GetRowMap().Num() - 1;
	const int32 RowIndex = FMath::RandRange(0, NumRows);

	TArray<FJSONData*> RandomRoomRows;
	TArray<FName> RandomRoomRowNames = Room->GetRowNames();
	Room->GetAllRows("", RandomRoomRows);

	RoomJSON = RandomRoomRows[RowIndex]->JSON;
	RoomName = RandomRoomRowNames[RowIndex].ToString();
}

void AMRUKSceneDataProvider::BeginPlay()
{
	Super::BeginPlay();
}

void AMRUKSceneDataProvider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
