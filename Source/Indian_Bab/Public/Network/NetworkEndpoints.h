#pragma once

#include "CoreMinimal.h"

/**
 * MM(매치메이커)·AC(안티치트) 엔드포인트 단일 소스.
 *
 * BaseURL은 빌드타임 하드코딩 — 값은 Indian_Bab.Build.cs의
 *   INDIANBAB_MM_BASE_URL / INDIANBAB_AC_BASE_URL
 * PublicDefinitions에서 주입됨. 운영 IP 변경 시 Build.cs 한 줄 수정 후 재빌드.
 *
 * 경로는 모두 여기서 관리. 외부(클라/데디) 호출처는 URL 조립 책임 없음.
 * 내부 S2S 경로(Internal 네임스페이스)는 WITH_SERVER_CODE 가드로
 * 클라 EXE(Type=Client)에는 컴파일조차 되지 않음.
 */

#ifndef INDIANBAB_MM_BASE_URL
    #error "INDIANBAB_MM_BASE_URL must be defined by Indian_Bab.Build.cs"
#endif
#ifndef INDIANBAB_AC_BASE_URL
    #error "INDIANBAB_AC_BASE_URL must be defined by Indian_Bab.Build.cs"
#endif

namespace NetworkEndpoints
{
namespace MM
{
    inline const FString& BaseURL()
    {
        static const FString V = TEXT(INDIANBAB_MM_BASE_URL);
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
        static const FString V = TEXT(INDIANBAB_AC_BASE_URL);
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
