// RedFront 1941 — Game Target
// 由 UE 5.8 的 UBT 读取；缺少本文件与 RedFront1941Editor.Target.cs 时工程无法编译。
using UnrealBuildTool;
using System.Collections.Generic;

public class RedFront1941Target : TargetRules
{
	public RedFront1941Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RedFront1941" });
	}
}
