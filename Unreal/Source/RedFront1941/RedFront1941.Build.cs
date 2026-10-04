// Copyright RedFront1941. All Rights Reserved.

using UnrealBuildTool;

/// <summary>
/// Runtime module for RedFront1941. Paper2D supplies the sprite/flipbook pipeline,
/// EnhancedInput supplies the 2D mouse-to-world aim bindings, Json/JsonUtilities
/// parse the shared data contracts under Content/RedFront/Data.
/// </summary>
public class RedFront1941 : ModuleRules
{
	public RedFront1941(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Paper2D",
			"Json",
			"JsonUtilities",
			"UMG",
			"Slate",
			"SlateCore",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI"
		});

		// Public headers of this module are included as "Core/RFGameMode.h", "Data/RFDataTypes.h", ...
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
