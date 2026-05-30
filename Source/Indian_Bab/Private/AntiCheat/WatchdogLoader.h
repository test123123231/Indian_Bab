#pragma once

#include "CoreMinimal.h"

/**
 * 워치독 DLL을 디스크의 암호화 페이로드(`wd.dat`)에서 메모리로 복호화 → PE 수동 매핑 →
 * DllMain 호출(WatchdogContext를 lpvReserved로 주입)까지 한 번에 처리하는 헬퍼.
 *
 * UAntiCheatSubsystem 전용. LoadLibrary 미사용 — 평문 사본은 매핑 직후 zeroize.
 */
struct FWatchdogStartParams
{
    FString UserId;        // SteamID (ASCII)
    FString SessionKeyB64; // verify 응답 그대로
    FString Host;          // "127.0.0.1"
    uint16  Port = 0;      // 9000
    bool    bUseTls = false;
};

namespace WatchdogLoader
{
    /**
     * 1회 호출. 성공 시 워치독 워커가 백그라운드 스레드에서 가동 중.
     * @param OutHandle 언로드 시 Unload()에 전달할 불투명 핸들(매핑 base + ctx 보관).
     */
    bool LoadAndStart(const TArray<uint8>& EncryptedBlob,
                      const TArray<uint8>& AesKey32,
                      const FWatchdogStartParams& Params,
                      void*& OutHandle);

    /** Deinitialize 경로. DllMain(DETACH) 호출 + VirtualFree. */
    void Unload(void*& Handle);
}
