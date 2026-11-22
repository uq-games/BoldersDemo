// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BoldersDemo : ModuleRules
{
	public BoldersDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem", "Niagara", "ProceduralMeshComponent", "SlateCore", "Slate" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "AnimGraphRuntime", "BlueprintGraph", "AssetTools", "EditorScriptingUtilities", "UMGEditor" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
