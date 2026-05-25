#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

/**
 * MM(매치메이커)·AC(안티치트) 엔드포인트 단일 소스.
 *
 * BaseURL 해석 우선순위 (최초 접근 시 1회 해석, 함수-로컬 static 캐시):
 *   1) 커맨드라인: -MMBaseURL=http://host:port / -ACBaseURL=...
 *   2) GGameIni `[Network]` 섹션: MMBaseURL=... / ACBaseURL=...
 *   3) 기본값: http://127.0.0.1:8000 (MM) / :9000 (AC) — 단일 머신 운영 기준
 *
 * 운영 IP 변경 시 재빌드 불필요 — 데디 실행 인자나 Game.ini 한 줄로 교체.
 * 경로는 모두 여기서 관리. 외부(클라/데디) 호출처는 URL 조립 책임 없음.
 */
namespace NetworkEndpoints
{
namespace Detail
{
    inline FString ResolveBase(const TCHAR* CmdToken, const TCHAR* IniKey, const TCHAR* DefaultURL)
    {
        FString Out;
        if (FParse::Value(FCommandLine::Get(), CmdToken, Out) && !Out.IsEmpty())
        {
            return Out;
        }
        if (GConfig && GConfig->GetString(TEXT("Network"), IniKey, Out, GGameIni) && !Out.IsEmpty())
        {
            return Out;
        }
        return FString(DefaultURL);
    }
}

namespace MM
{
    inline const FString& BaseURL()
    {
        static const FString V = Detail::ResolveBase(
            TEXT("MMBaseURL="), TEXT("MMBaseURL"), TEXT("http://127.0.0.1:8000"));
        return V;
    }

    // 외부 공개 (Nginx /api/* → :8000)
    namespace External
    {
        inline FString CreateMatch()
        {
            return BaseURL() + TEXT("/api/match/create");
        }
        inline FString MatchAddress(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/api/match/%s/address"), *MatchId);
        }
    }

#if WITH_SERVER_CODE
    // 내부 S2S (loopback, Nginx deny — 데디→MM).
    // 클라 EXE(Type=Client, WITH_SERVER_CODE=0)에는 경로 리터럴조차 박지 않음.
    namespace Internal
    {
        inline FString Host(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/internal/match/%s/host"), *MatchId);
        }
        inline FString Close(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/internal/match/%s/close"), *MatchId);
        }
        inline FString ClearHost(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/internal/match/%s/clear_host"), *MatchId);
        }
    }
#endif
}

namespace AC
{
    inline const FString& BaseURL()
    {
        static const FString V = Detail::ResolveBase(
            TEXT("ACBaseURL="), TEXT("ACBaseURL"), TEXT("http://127.0.0.1:9000"));
        return V;
    }

    // 외부 공개 (클라→AC)
    namespace External
    {
        inline FString Verify()
        {
            return BaseURL() + TEXT("/api/verify");
        }
    }

#if WITH_SERVER_CODE
    // 내부 S2S (loopback — 데디→AC). 클라 EXE 비노출.
    namespace Internal
    {
        inline FString PreLogin()
        {
            return BaseURL() + TEXT("/internal/anc/prelogin");
        }
        inline FString Leave()
        {
            return BaseURL() + TEXT("/internal/anc/leave");
        }
    }
#endif
}
}
