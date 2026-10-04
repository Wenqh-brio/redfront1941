// RedFront 1941 — Editor Target
// 编辑器目标：RedFront1941Editor Win64 Development
using UnrealBuildTool;
using System.Collections.Generic;

public class RedFront1941EditorTarget : TargetRules
{
	public RedFront1941EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RedFront1941" });
	}
}
