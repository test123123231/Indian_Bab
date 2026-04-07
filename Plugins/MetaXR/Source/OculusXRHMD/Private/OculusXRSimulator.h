// @lint-ignore-every LICENSELINT
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaXRSim, Log, All);

/**  */
class FMetaXRSimulator
{
public:
	static FMetaXRSimulator& Get()
	{
		static FMetaXRSimulator instance;
		return instance;
	}
	FMetaXRSimulator(const FMetaXRSimulator&) = delete;
	FMetaXRSimulator& operator=(const FMetaXRSimulator&) = delete;

	bool IsSimulatorActivated();
	void ToggleOpenXRRuntime();
	FString GetPackagePath() const;
	bool IsSimulatorInstalled() const;
	TArray<FString> GetInstalledVersions() const;
	static bool IsXRSim2Installed();

private:
	struct FMetaXRSimulatorVersion
	{
		FString DownloadUrl;
		FString Version;
		double UrlValidity;
	};

	static void SpawnNotificationToUpdateIfAvailable();
	void FetchAvailableVersions();
	FMetaXRSimulator();
	~FMetaXRSimulator() = default;
	FString GetSimulatorJsonPath() const;
	void InstallPortableSimulator(const FString& URL, const FString& Version, TFunction<void()> OnSuccess);
	void UnzipSimulator(const FString& Path, const FString& TargetPath, const TSharedPtr<SNotificationItem>& Notification, TFunction<void()> OnSuccess);

	const FString OldInstallationPath;

	TArray<FMetaXRSimulatorVersion> AvailableVersions;
};
#endif
