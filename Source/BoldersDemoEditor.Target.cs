// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class BoldersDemoEditorTarget : TargetRules
{
	public BoldersDemoEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("BoldersDemo");
	}
}
