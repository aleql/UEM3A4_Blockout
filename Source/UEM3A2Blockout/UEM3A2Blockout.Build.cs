// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEM3A2Blockout : ModuleRules
{
	public UEM3A2Blockout(ReadOnlyTargetRules Target) : base(Target)
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
			"Paper2D"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"UEM3A2Blockout",
			"UEM3A2Blockout/Variant_Platforming",
			"UEM3A2Blockout/Variant_Platforming/Animation",
			"UEM3A2Blockout/Variant_Combat",
			"UEM3A2Blockout/Variant_Combat/AI",
			"UEM3A2Blockout/Variant_Combat/Animation",
			"UEM3A2Blockout/Variant_Combat/Gameplay",
			"UEM3A2Blockout/Variant_Combat/Interfaces",
			"UEM3A2Blockout/Variant_Combat/UI",
			"UEM3A2Blockout/Variant_SideScrolling",
			"UEM3A2Blockout/Variant_SideScrolling/AI",
			"UEM3A2Blockout/Variant_SideScrolling/Gameplay",
			"UEM3A2Blockout/Variant_SideScrolling/Interfaces",
			"UEM3A2Blockout/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
