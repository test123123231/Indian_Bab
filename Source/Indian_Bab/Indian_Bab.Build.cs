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
            "UMG",
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "RHI",
            "OnlineSubsystemUtils",
            "OnlineSubsystem",
            "HairStrandsCore",
            "Niagara",
            "HTTP",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

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
