// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CraftIslandPocket3 : ModuleRules
{
	public CraftIslandPocket3(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Dojo", "UMG", "Paper2D" });
	}
}
