// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LOAEditorTarget : TargetRules
{
	public LOAEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("LOA");

		// 에디터 전용 도구 모듈 (스킬 만들기 등). 게임 타깃(LOA.Target.cs)에는 넣지 않는다 → 패키징한 게임에 안 들어감
		ExtraModuleNames.Add("LOAEditor");
	}
}
