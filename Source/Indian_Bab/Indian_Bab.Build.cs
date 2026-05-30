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

        if (Target.Type != TargetType.Server)
        {
            DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
        }

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
        // MM/AC 엔드포인트 — host/port/tls를 1차 SSoT로 노출, BaseURL은 헤더에서 파생 조립.
        // 운영 IP/포트/스킴 변경은 여기서 수정 후 재빌드.
        // 헤더(Public/Network/NetworkEndpoints.h)가 이 매크로를 읽어 host/port/tls/BaseURL 노출.
        // ─────────────────────────────────────────────────────────────
        string MMHost  = "127.0.0.1"; int MMPort  = 8000; int MMUseTls = 0;
        string ACHost  = "127.0.0.1"; int ACPort  = 9000; int ACUseTls = 0;

        PublicDefinitions.Add("INDIANBAB_MM_HOST=\""  + MMHost + "\"");
        PublicDefinitions.Add("INDIANBAB_MM_PORT="    + MMPort);
        PublicDefinitions.Add("INDIANBAB_MM_USE_TLS=" + MMUseTls);
        PublicDefinitions.Add("INDIANBAB_AC_HOST=\""  + ACHost + "\"");
        PublicDefinitions.Add("INDIANBAB_AC_PORT="    + ACPort);
        PublicDefinitions.Add("INDIANBAB_AC_USE_TLS=" + ACUseTls);

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
