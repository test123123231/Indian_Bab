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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Indian_Bab",
			"Indian_Bab/Variant_Platforming",
			"Indian_Bab/Variant_Platforming/Animation",
			"Indian_Bab/Variant_Combat",
			"Indian_Bab/Variant_Combat/AI",
			"Indian_Bab/Variant_Combat/Animation",
			"Indian_Bab/Variant_Combat/Gameplay",
			"Indian_Bab/Variant_Combat/Interfaces",
			"Indian_Bab/Variant_Combat/UI",
			"Indian_Bab/Variant_SideScrolling",
			"Indian_Bab/Variant_SideScrolling/AI",
			"Indian_Bab/Variant_SideScrolling/Gameplay",
			"Indian_Bab/Variant_SideScrolling/Interfaces",
			"Indian_Bab/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
