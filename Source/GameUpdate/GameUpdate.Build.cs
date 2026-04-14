// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameUpdate : ModuleRules
{
	public GameUpdate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "HotUpdate" });

		PublicIncludePaths.AddRange(new string[] {
			"GameUpdate",
			"GameUpdate/Variant_Strategy",
			"GameUpdate/Variant_Strategy/UI",
			"GameUpdate/Variant_TwinStick",
			"GameUpdate/Variant_TwinStick/AI",
			"GameUpdate/Variant_TwinStick/Gameplay",
			"GameUpdate/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
