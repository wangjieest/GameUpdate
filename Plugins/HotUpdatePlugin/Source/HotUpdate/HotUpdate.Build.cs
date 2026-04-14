// Copyright czm. All Rights Reserved.

using UnrealBuildTool;

public class HotUpdate : ModuleRules
{
	public HotUpdate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HTTP",
			"PakFile",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"AssetRegistry"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"ApplicationCore"
		});
	}
}