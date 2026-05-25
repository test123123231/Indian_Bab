// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Indian_Bab : ModuleRules
{
    public Indian_Bab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AIModule",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "HTTP",
            "Json",
            "JsonUtilities",
            "UMG",
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "RHI",
            "OnlineSubsystemUtils",
            "OnlineSubsystem",
            "HairStrandsCore",
            "Niagara",
            "HeadMountedDisplay",
            "XRBase"
        });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Steamworks SDK: SteamCredentials가 GetAuthSessionTicket 콜백 동기화를 위해 직접 호출.
        // 엔진 동봉 SDK 사용
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

        //   bcrypt.lib: AntiCheatSubsystem SHA-256 (클라 부팅 verify)
        //   ole32.lib : ConnectivitySubsystem INetworkListManager COM (클라 NLM 폴링)
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("bcrypt.lib");
            PublicSystemLibraries.Add("ole32.lib");
        }

        // 전역 빌드 플래그 — Dev/Debug에서 켜고 끄는 mock 토글.
        // Shipping은 강제 0(실통신)으로 잠궈 사고 방지.
        int MatchmakerUseMock = 0;
        int AntiCheatUseMock  = 1;

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            MatchmakerUseMock = 0;
            AntiCheatUseMock  = 0;
        }

        PublicDefinitions.Add("MATCHMAKER_USE_MOCK=" + MatchmakerUseMock);
        PublicDefinitions.Add("ANTICHEAT_USE_MOCK=" + AntiCheatUseMock);

        // ─────────────────────────────────────────────────────────────
        // MM/AC 엔드포인트 base URL — 운영 IP 변경 시 여기서 한 줄 수정 후 재빌드.
        // 헤더(Public/Network/NetworkEndpoints.h)가 이 매크로를 읽어 BaseURL로 사용.
        // 값은 따옴표 escape 필요 — PublicDefinitions는 raw 문자열로 전처리기에 주입됨.
        // 타겟별/Configuration별 분기가 필요해지면 여기서 분기.
        // ─────────────────────────────────────────────────────────────
        string MMBaseURL = "http://127.0.0.1:8000";
        string ACBaseURL = "http://127.0.0.1:9000";

        PublicDefinitions.Add("INDIANBAB_MM_BASE_URL=\"" + MMBaseURL + "\"");
        PublicDefinitions.Add("INDIANBAB_AC_BASE_URL=\"" + ACBaseURL + "\"");

        PublicIncludePaths.AddRange(new string[] {
            "Indian_Bab",
            "Indian_Bab/TP_FirstPerson",
            "Indian_Bab/TP_FirstPerson/Variant_Horror",
            "Indian_Bab/TP_FirstPerson/Variant_Horror/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/AI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/Weapons"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
