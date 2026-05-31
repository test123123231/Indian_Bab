#include "AntiCheat/WatchdogLoader.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogWatchdogLoader, Log, All);

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <winnt.h>
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace
{
    static constexpr uint8 kMagic[4] = { 'W', 'D', '0', '1' };
    static constexpr ULONG kIvBytes  = 12;
    static constexpr ULONG kTagBytes = 16;

    // 매핑된 워치독 인스턴스 1개 분의 보존 자료. UAntiCheatSubsystem이 void*로 들고 다님.
    struct FMapped
    {
        uint8* Base = nullptr;
        SIZE_T SizeOfImage = 0;
        // DllMain 호출 시 lpvReserved로 넘긴 컨텍스트가 워커 스레드에서 비동기로 참조될 수 있어
        // 문자열 백킹은 객체 lifetime 동안 살려 둔다.
        FTCHARToUTF8 UserIdUtf8;
        FTCHARToUTF8 SessionKeyUtf8;
        FTCHARToUTF8 HostUtf8;
        // Watchdog/include/Watchdog.h의 WatchdogContext와 정확히 일치해야 함.
        // 한쪽 ABI 수정 시 양쪽 동시 변경 필수.
        using WatchdogTerminateCb = void (__cdecl*)(const char* reason, void* user);
        struct WatchdogContext
        {
            uint32_t            struct_size;
            const char*         user_id;
            const char*         session_key_b64;
            const char*         host;
            uint16_t            port;
            uint16_t            use_tls;
            WatchdogTerminateCb on_terminate;
            void*               on_terminate_user;
        } Ctx{};

        FMapped(const FString& U, const FString& S, const FString& H)
            : UserIdUtf8(*U), SessionKeyUtf8(*S), HostUtf8(*H) {}
    };

    // 워치독 worker 스레드 컨텍스트에서 호출됨 → GameThread로 marshal한 뒤
    // 안티치트 종료 모달 표시 + 강제 종료. reason은 콜백 반환 전까지만 유효하므로 즉시 복사.
    void __cdecl OnWatchdogTerminate(const char* reason, void* /*user*/)
    {
        FString ReasonCopy = reason ? FString(UTF8_TO_TCHAR(reason)) : FString(TEXT(""));
        UE_LOG(LogWatchdogLoader, Warning,
            TEXT("[Watchdog] TERMINATE callback received: reason=%s"), *ReasonCopy);

        // GameThread는 우리 모달/RequestExit이 안전하게 동작하는 컨텍스트.
        AsyncTask(ENamedThreads::GameThread, [ReasonCopy]()
        {
            const FString Body = FString::Printf(
                TEXT("안티치트 서버로부터 종료 신호를 받았습니다.\n사유: %s\n게임을 종료합니다."),
                ReasonCopy.IsEmpty() ? TEXT("(없음)") : *ReasonCopy);
#if PLATFORM_WINDOWS
            ::MessageBoxW(nullptr, *Body,
                L"Indian_Bab — 안티치트 종료 신호",
                MB_OK | MB_ICONERROR);
#endif
            UE_LOG(LogWatchdogLoader, Error,
                TEXT("[Watchdog] TERMINATE — 게임 종료."));
            FPlatformMisc::RequestExit(/*bForce=*/true);
        });
    }

    bool DecryptGCM(const TArray<uint8>& Blob,
                    const TArray<uint8>& Key,
                    TArray<uint8>& OutPlain)
    {
        if (Blob.Num() < (int32)(sizeof(kMagic) + kIvBytes + kTagBytes))
        {
            UE_LOG(LogWatchdogLoader, Error, TEXT("wd.dat 크기 부족 (%d)"), Blob.Num());
            return false;
        }
        if (FMemory::Memcmp(Blob.GetData(), kMagic, sizeof(kMagic)) != 0)
        {
            UE_LOG(LogWatchdogLoader, Error, TEXT("wd.dat magic 불일치"));
            return false;
        }
        if (Key.Num() != 32)
        {
            UE_LOG(LogWatchdogLoader, Error, TEXT("AES key 길이 %d (32 필요)"), Key.Num());
            return false;
        }

        const uint8* Iv  = Blob.GetData() + sizeof(kMagic);
        const uint8* Tag = Iv + kIvBytes;
        const uint8* Ct  = Tag + kTagBytes;
        const ULONG  CtLen = (ULONG)(Blob.Num() - sizeof(kMagic) - kIvBytes - kTagBytes);

        BCRYPT_ALG_HANDLE Alg = nullptr;
        BCRYPT_KEY_HANDLE KeyH = nullptr;
        NTSTATUS Status = BCryptOpenAlgorithmProvider(&Alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!NT_SUCCESS(Status))
        {
            UE_LOG(LogWatchdogLoader, Error, TEXT("BCryptOpenAlgorithmProvider 0x%08x"), Status);
            return false;
        }
        Status = BCryptSetProperty(Alg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (!NT_SUCCESS(Status))
        {
            BCryptCloseAlgorithmProvider(Alg, 0);
            UE_LOG(LogWatchdogLoader, Error, TEXT("BCryptSetProperty(GCM) 0x%08x"), Status);
            return false;
        }
        Status = BCryptGenerateSymmetricKey(Alg, &KeyH, nullptr, 0,
            (PUCHAR)Key.GetData(), (ULONG)Key.Num(), 0);
        if (!NT_SUCCESS(Status))
        {
            BCryptCloseAlgorithmProvider(Alg, 0);
            UE_LOG(LogWatchdogLoader, Error, TEXT("BCryptGenerateSymmetricKey 0x%08x"), Status);
            return false;
        }

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO Info;
        BCRYPT_INIT_AUTH_MODE_INFO(Info);
        Info.pbNonce = (PUCHAR)Iv;
        Info.cbNonce = kIvBytes;
        Info.pbTag   = (PUCHAR)Tag;
        Info.cbTag   = kTagBytes;

        OutPlain.SetNumUninitialized(CtLen);
        ULONG Produced = 0;
        Status = BCryptDecrypt(KeyH,
            (PUCHAR)Ct, CtLen,
            &Info,
            nullptr, 0,
            OutPlain.GetData(), CtLen,
            &Produced,
            0);

        BCryptDestroyKey(KeyH);
        BCryptCloseAlgorithmProvider(Alg, 0);

        if (!NT_SUCCESS(Status))
        {
            // STATUS_AUTH_TAG_MISMATCH = 0xC000A002
            UE_LOG(LogWatchdogLoader, Error, TEXT("BCryptDecrypt 0x%08x (tag mismatch?)"), Status);
            FMemory::Memzero(OutPlain.GetData(), OutPlain.Num());
            OutPlain.Empty();
            return false;
        }
        OutPlain.SetNum((int32)Produced, EAllowShrinking::No);
        return true;
    }

    // PE 파일 이미지를 메모리에 수동 매핑. 성공 시 OutBase = VirtualAlloc된 베이스, OutSize = SizeOfImage.
    bool MapPEInMemory(const TArray<uint8>& Plain, uint8*& OutBase, SIZE_T& OutSize)
    {
        if (Plain.Num() < (int32)sizeof(IMAGE_DOS_HEADER))
        {
            return false;
        }
        const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Plain.GetData());
        if (Dos->e_magic != IMAGE_DOS_SIGNATURE) { return false; }
        if (Dos->e_lfanew <= 0 || Dos->e_lfanew + (LONG)sizeof(IMAGE_NT_HEADERS64) > Plain.Num())
        {
            return false;
        }
        const auto* Nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Plain.GetData() + Dos->e_lfanew);
        if (Nt->Signature != IMAGE_NT_SIGNATURE) { return false; }
        if (Nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) { return false; }
        if (Nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { return false; }

        const SIZE_T ImageSize = Nt->OptionalHeader.SizeOfImage;
        uint8* Base = (uint8*)VirtualAlloc(nullptr, ImageSize,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!Base) { return false; }

        // 헤더 복사
        FMemory::Memcpy(Base, Plain.GetData(), Nt->OptionalHeader.SizeOfHeaders);

        // 섹션 복사
        const auto* Sec = IMAGE_FIRST_SECTION(Nt);
        for (WORD i = 0; i < Nt->FileHeader.NumberOfSections; ++i)
        {
            if (Sec[i].SizeOfRawData > 0)
            {
                FMemory::Memcpy(Base + Sec[i].VirtualAddress,
                    Plain.GetData() + Sec[i].PointerToRawData,
                    Sec[i].SizeOfRawData);
            }
        }

        // 베이스 릴로케이션
        const intptr_t Delta = (intptr_t)Base - (intptr_t)Nt->OptionalHeader.ImageBase;
        if (Delta != 0)
        {
            const IMAGE_DATA_DIRECTORY& RelocDir =
                Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            if (RelocDir.Size > 0 && RelocDir.VirtualAddress != 0)
            {
                auto* Block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(Base + RelocDir.VirtualAddress);
                const uint8* End = (uint8*)Block + RelocDir.Size;
                while ((uint8*)Block < End && Block->SizeOfBlock > 0)
                {
                    const DWORD Count = (Block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                    const WORD* List = reinterpret_cast<const WORD*>(Block + 1);
                    for (DWORD k = 0; k < Count; ++k)
                    {
                        const WORD Type = List[k] >> 12;
                        const WORD Offset = List[k] & 0x0FFF;
                        if (Type == IMAGE_REL_BASED_DIR64)
                        {
                            uint64* P = reinterpret_cast<uint64*>(Base + Block->VirtualAddress + Offset);
                            *P += (uint64)Delta;
                        }
                        else if (Type == IMAGE_REL_BASED_ABSOLUTE)
                        {
                            // 패딩 — 무시
                        }
                        else
                        {
                            UE_LOG(LogWatchdogLoader, Error, TEXT("unsupported reloc type %u"), Type);
                            VirtualFree(Base, 0, MEM_RELEASE);
                            return false;
                        }
                    }
                    Block = reinterpret_cast<IMAGE_BASE_RELOCATION*>((uint8*)Block + Block->SizeOfBlock);
                }
            }
        }

        // IAT
        const IMAGE_DATA_DIRECTORY& ImpDir =
            Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (ImpDir.Size > 0 && ImpDir.VirtualAddress != 0)
        {
            auto* Imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(Base + ImpDir.VirtualAddress);
            for (; Imp->Name != 0; ++Imp)
            {
                const char* ModName = reinterpret_cast<const char*>(Base + Imp->Name);
                HMODULE H = GetModuleHandleA(ModName);
                if (!H) { H = LoadLibraryA(ModName); }
                if (!H)
                {
                    UE_LOG(LogWatchdogLoader, Error, TEXT("import LoadLibraryA failed: %hs"), ModName);
                    VirtualFree(Base, 0, MEM_RELEASE);
                    return false;
                }

                const DWORD LookupRva = Imp->OriginalFirstThunk ? Imp->OriginalFirstThunk : Imp->FirstThunk;
                auto* Lookup = reinterpret_cast<IMAGE_THUNK_DATA64*>(Base + LookupRva);
                auto* IAT    = reinterpret_cast<IMAGE_THUNK_DATA64*>(Base + Imp->FirstThunk);
                for (; Lookup->u1.AddressOfData != 0; ++Lookup, ++IAT)
                {
                    FARPROC Proc = nullptr;
                    if (IMAGE_SNAP_BY_ORDINAL64(Lookup->u1.Ordinal))
                    {
                        Proc = GetProcAddress(H, (LPCSTR)IMAGE_ORDINAL64(Lookup->u1.Ordinal));
                    }
                    else
                    {
                        auto* ByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                            Base + (DWORD)Lookup->u1.AddressOfData);
                        Proc = GetProcAddress(H, ByName->Name);
                    }
                    if (!Proc)
                    {
                        UE_LOG(LogWatchdogLoader, Error, TEXT("GetProcAddress failed in %hs"), ModName);
                        VirtualFree(Base, 0, MEM_RELEASE);
                        return false;
                    }
                    IAT->u1.Function = (ULONGLONG)Proc;
                }
            }
        }

        // 섹션 보호
        for (WORD i = 0; i < Nt->FileHeader.NumberOfSections; ++i)
        {
            const DWORD Ch = Sec[i].Characteristics;
            const bool R = (Ch & IMAGE_SCN_MEM_READ)    != 0;
            const bool W = (Ch & IMAGE_SCN_MEM_WRITE)   != 0;
            const bool X = (Ch & IMAGE_SCN_MEM_EXECUTE) != 0;
            DWORD Protect = PAGE_NOACCESS;
            if      ( X &&  R &&  W) Protect = PAGE_EXECUTE_READWRITE;
            else if ( X &&  R)       Protect = PAGE_EXECUTE_READ;
            else if ( X)             Protect = PAGE_EXECUTE;
            else if ( R &&  W)       Protect = PAGE_READWRITE;
            else if ( R)             Protect = PAGE_READONLY;
            DWORD Old = 0;
            const SIZE_T Size = Sec[i].Misc.VirtualSize > 0 ? Sec[i].Misc.VirtualSize : Sec[i].SizeOfRawData;
            if (Size > 0)
            {
                VirtualProtect(Base + Sec[i].VirtualAddress, Size, Protect, &Old);
            }
        }

        FlushInstructionCache(GetCurrentProcess(), Base, ImageSize);

        OutBase = Base;
        OutSize = ImageSize;
        return true;
    }
}

bool WatchdogLoader::LoadAndStart(const TArray<uint8>& EncryptedBlob,
                                  const TArray<uint8>& AesKey32,
                                  const FWatchdogStartParams& Params,
                                  void*& OutHandle)
{
    OutHandle = nullptr;

    TArray<uint8> Plain;
    if (!DecryptGCM(EncryptedBlob, AesKey32, Plain))
    {
        return false;
    }

    uint8* Base = nullptr;
    SIZE_T Size = 0;
    const bool bMapped = MapPEInMemory(Plain, Base, Size);

    // 평문 PE는 매핑 직후 zeroize — 디스크/메모리 양쪽에 사본 잔류 금지.
    FMemory::Memzero(Plain.GetData(), Plain.Num());
    Plain.Empty();

    if (!bMapped)
    {
        UE_LOG(LogWatchdogLoader, Error, TEXT("PE 수동 매핑 실패"));
        return false;
    }

    auto* Mapped = new FMapped(Params.UserId, Params.SessionKeyB64, Params.Host);
    Mapped->Base = Base;
    Mapped->SizeOfImage = Size;
    Mapped->Ctx.struct_size       = (uint32_t)sizeof(Mapped->Ctx);
    Mapped->Ctx.user_id           = Mapped->UserIdUtf8.Get();
    Mapped->Ctx.session_key_b64   = Mapped->SessionKeyUtf8.Get();
    Mapped->Ctx.host              = Mapped->HostUtf8.Get();
    Mapped->Ctx.port              = Params.Port;
    Mapped->Ctx.use_tls           = Params.bUseTls ? 1 : 0;
    Mapped->Ctx.on_terminate      = &OnWatchdogTerminate;
    Mapped->Ctx.on_terminate_user = nullptr;

    // DllMain 호출
    const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
    const auto* Nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Base + Dos->e_lfanew);

    // TLS 콜백 (안전망 — 워치독은 TLS 미사용)
    {
        const auto& TlsDir = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
        if (TlsDir.Size > 0 && TlsDir.VirtualAddress != 0)
        {
            const auto* Tls = reinterpret_cast<const IMAGE_TLS_DIRECTORY64*>(Base + TlsDir.VirtualAddress);
            if (Tls->AddressOfCallBacks != 0)
            {
                auto** Cbs = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(Tls->AddressOfCallBacks);
                for (; *Cbs; ++Cbs)
                {
                    (*Cbs)((PVOID)Base, DLL_PROCESS_ATTACH, &Mapped->Ctx);
                }
            }
        }
    }

    using DllEntryFn = BOOL (WINAPI*)(HINSTANCE, DWORD, LPVOID);
    auto* Entry = reinterpret_cast<DllEntryFn>(Base + Nt->OptionalHeader.AddressOfEntryPoint);

    UE_LOG(LogWatchdogLoader, Log,
        TEXT("[Watchdog] mapped base=0x%p size=%llu entry_rva=0x%x"),
        Base, (uint64)Size, Nt->OptionalHeader.AddressOfEntryPoint);

    const BOOL bAttach = Entry((HINSTANCE)Base, DLL_PROCESS_ATTACH, &Mapped->Ctx);
    if (!bAttach)
    {
        UE_LOG(LogWatchdogLoader, Error, TEXT("[Watchdog] DllMain(ATTACH) returned FALSE"));
        VirtualFree(Base, 0, MEM_RELEASE);
        delete Mapped;
        return false;
    }

    OutHandle = Mapped;
    return true;
}

void WatchdogLoader::Unload(void*& Handle)
{
    if (!Handle) { return; }
    auto* Mapped = static_cast<FMapped*>(Handle);

    const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Mapped->Base);
    const auto* Nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Mapped->Base + Dos->e_lfanew);
    using DllEntryFn = BOOL (WINAPI*)(HINSTANCE, DWORD, LPVOID);
    auto* Entry = reinterpret_cast<DllEntryFn>(Mapped->Base + Nt->OptionalHeader.AddressOfEntryPoint);
    Entry((HINSTANCE)Mapped->Base, DLL_PROCESS_DETACH, nullptr);

    VirtualFree(Mapped->Base, 0, MEM_RELEASE);
    delete Mapped;
    Handle = nullptr;
}

#else // !PLATFORM_WINDOWS

bool WatchdogLoader::LoadAndStart(const TArray<uint8>&, const TArray<uint8>&,
                                  const FWatchdogStartParams&, void*& OutHandle)
{
    OutHandle = nullptr;
    return false;
}
void WatchdogLoader::Unload(void*& Handle) { Handle = nullptr; }

#endif
