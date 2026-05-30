// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Indian_BabTarget : TargetRules
{
	public Indian_BabTarget(TargetInfo Target) : base(Target)
	{
		// 데디 전용 운영(리슨/P2P 미지원) — Client 타겟으로 WITH_SERVER_CODE=0 강제,
		// 서버 전용 코드(#if WITH_SERVER_CODE)를 클라 EXE에서 컴파일 단계에 제거.
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("Indian_Bab");
	}
}
