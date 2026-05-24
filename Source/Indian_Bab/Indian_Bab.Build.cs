// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Indian_Bab : ModuleRules
{
    public Indian_Bab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AIModule",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "HTTP",
            "Json",
            "JsonUtilities",
            "UMG",
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "RHI",
            "OnlineSubsystemUtils",
            "OnlineSubsystem",
            "HairStrandsCore",
            "Niagara",
            "HeadMountedDisplay",
            "XRBase"
        });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        PrivateDependencyModuleNames.AddRange(new string[] { });

        //   bcrypt.lib: AntiCheatSubsystem SHA-256 (클라 부팅 verify)
        //   ole32.lib : ConnectivitySubsystem INetworkListManager COM (클라 NLM 폴링)
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("bcrypt.lib");
            PublicSystemLibraries.Add("ole32.lib");
        }

        PublicIncludePaths.AddRange(new string[] {
            "Indian_Bab",
            "Indian_Bab/TP_FirstPerson",
            "Indian_Bab/TP_FirstPerson/Variant_Horror",
            "Indian_Bab/TP_FirstPerson/Variant_Horror/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/AI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/Weapons"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
