#include "GameInstanceSubsystem/SteamCredentialsSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#include "OnlineSubsystem.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogSteamCredentials, Log, All);

namespace
{
    // GetAuthSessionTicket 호출 후 GetAuthSessionTicketResponse_t 콜백 도착까지
    // 대기. self-managed — Finish 안에서 delete this.
    // OSS Steam이 매 틱 SteamAPI_RunCallbacks를 돌리므로 별도 펌프 불필요.
    class FTicketRequest
    {
    public:
        explicit FTicketRequest(USteamCredentialsSubsystem::FCallback InCallback)
            : Callback(MoveTemp(InCallback))
            , CallbackResponse(this, &FTicketRequest::OnTicketResponse)
        {
        }

        void Start()
        {
            ISteamUser* User = SteamUser();
            if (!User)
            {
                UE_LOG(LogSteamCredentials, Error, TEXT("SteamUser() null — Steam OSS 미초기화"));
                Finish(false);
                return;
            }

            const CSteamID Id = User->GetSteamID();
            if (!Id.IsValid() || !Id.BIndividualAccount())
            {
                UE_LOG(LogSteamCredentials, Error, TEXT("CSteamID invalid — Steam 미로그인"));
                Finish(false);
                return;
            }
            SteamIdStr = FString::Printf(TEXT("%llu"),
                static_cast<unsigned long long>(Id.ConvertToUint64()));

            uint8 Buf[1024] = {};
            uint32 Len = 0;
            Handle = User->GetAuthSessionTicket(Buf, sizeof(Buf), &Len, nullptr);
            if (Handle == k_HAuthTicketInvalid || Len == 0)
            {
                UE_LOG(LogSteamCredentials, Error, TEXT("GetAuthSessionTicket 실패"));
                Finish(false);
                return;
            }
            TicketHex = BytesToHex(Buf, static_cast<int32>(Len));  // UE 기본 uppercase

            // 5s 타임아웃 가드 (Steam 백엔드 응답 지연/네트워크 장애 대비)
            TWeakPtr<int> WeakAlive = AliveToken;
            TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([this, WeakAlive](float) -> bool
                {
                    if (WeakAlive.IsValid())
                    {
                        UE_LOG(LogSteamCredentials, Warning, TEXT("ticket 콜백 5s 타임아웃"));
                        Finish(false);
                    }
                    return false;
                }), 5.0f);
        }

    private:
        void OnTicketResponse(GetAuthSessionTicketResponse_t* Param)
        {
            if (Param->m_hAuthTicket != Handle) return;
            if (Param->m_eResult != k_EResultOK)
            {
                UE_LOG(LogSteamCredentials, Error, TEXT("ticket result=%d (OK 아님)"),
                    static_cast<int32>(Param->m_eResult));
                Finish(false);
                return;
            }
            Finish(true);
        }

        void Finish(bool bSuccess)
        {
            if (bDone) return;
            bDone = true;

            if (TimeoutHandle.IsValid())
            {
                FTSTicker::RemoveTicker(TimeoutHandle);
                TimeoutHandle.Reset();
            }

            if (!bSuccess && Handle != k_HAuthTicketInvalid)
            {
                if (ISteamUser* User = SteamUser())
                {
                    User->CancelAuthTicket(Handle);
                }
                Handle = k_HAuthTicketInvalid;
            }

            const FString OutTicket = bSuccess ? TicketHex : FString();
            const FString OutId = bSuccess ? SteamIdStr : FString();
            USteamCredentialsSubsystem::FCallback Local = MoveTemp(Callback);
            AliveToken.Reset();
            delete this;
            Local(bSuccess, OutTicket, OutId);
        }

        USteamCredentialsSubsystem::FCallback Callback;
        CCallback<FTicketRequest, GetAuthSessionTicketResponse_t> CallbackResponse;
        HAuthTicket Handle = k_HAuthTicketInvalid;
        FString TicketHex;
        FString SteamIdStr;
        FTSTicker::FDelegateHandle TimeoutHandle;
        // Finish 직후 self-delete됨 — 타이머 람다는 이걸 weak으로 잡아 사후 호출 차단.
        TSharedPtr<int> AliveToken = MakeShared<int>(0);
        bool bDone = false;
    };

    // Standalone/패키지 빌드에서 Steam 부팅 가드가 실패하면 모달 띄우고 종료.
    // PIE/에디터는 호출하지 않음 (lenient).
    void FatalExitSteamMissing(const TCHAR* Reason)
    {
#if PLATFORM_WINDOWS
        ::MessageBoxW(nullptr,
            L"Steam 클라이언트가 실행 중인지 확인하세요.\n게임을 종료합니다.",
            L"Indian_Bab — Steam 초기화 실패",
            MB_OK | MB_ICONERROR);
#endif
        UE_LOG(LogSteamCredentials, Error, TEXT("Steam init failed — %s. Exiting."), Reason);
        FPlatformMisc::RequestExit(false);
    }
}

bool USteamCredentialsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // 데디 서버는 Steam 클라 인증 토큰을 발급할 일이 없음.
    if (IsRunningDedicatedServer())
    {
        return false;
    }
    return Super::ShouldCreateSubsystem(Outer);
}

void USteamCredentialsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // GIsEditor: PIE 자식 월드 true / Standalone·패키지 false.
    // PIE는 NULL OSS로 떨어질 수 있으므로 lenient(bAvailable=false), Standalone/패키지는 Steam 강제.
    if (GIsEditor)
    {
        InitializeForEditor();
    }
    else
    {
        InitializeForRuntime();
    }
}

void USteamCredentialsSubsystem::InitializeForEditor()
{
    // PIE는 OSS 모듈 자체는 떠있되 이름이 "NULL"(NULL OSS)로 떨어지는 게 본 프로젝트의 정책.
    // → bAvailable=false로 Steam ticket 발급은 차단(NULL OSS는 SteamWorks 없음)하되,
    //    OSS::Get() 자체는 정상 호출 여부 검증해 둠 (모듈 로드 실패는 PIE에서도 비정상).
    // SessionSubsystem은 NULL OSS의 SessionInterface stub을 그대로 가져다 쓸 수 있음.
    bAvailable = false;

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS == nullptr)
    {
        UE_LOG(LogSteamCredentials, Error,
            TEXT("[Editor] IOnlineSubsystem::Get() == null — OSS 모듈 로드 실패"));
        return;
    }

    UE_LOG(LogSteamCredentials, Log,
        TEXT("[Editor] OSS 로드 확인 (이름=%s). Steam ticket 발급 비활성"),
        *OSS->GetSubsystemName().ToString());
}

void USteamCredentialsSubsystem::InitializeForRuntime()
{
    UE_LOG(LogSteamCredentials, Log, TEXT("[Runtime] Steam 부팅 가드 시작."));

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS == nullptr)
    {
        FatalExitSteamMissing(TEXT("IOnlineSubsystem::Get() null"));
        return;
    }

    const FName SubsystemName = OSS->GetSubsystemName();
    if (SubsystemName != FName(TEXT("STEAM")))
    {
        UE_LOG(LogSteamCredentials, Error,
            TEXT("[Runtime] OSS 이름 불일치: %s (기대 STEAM)"), *SubsystemName.ToString());
        FatalExitSteamMissing(TEXT("subsystem name mismatch"));
        return;
    }

    if (SteamUser() == nullptr)
    {
        FatalExitSteamMissing(TEXT("SteamUser() null"));
        return;
    }

    const CSteamID Id = SteamUser()->GetSteamID();
    if (!Id.IsValid() || !Id.BIndividualAccount())
    {
        FatalExitSteamMissing(TEXT("CSteamID invalid (미로그인)"));
        return;
    }

    bAvailable = true;
    UE_LOG(LogSteamCredentials, Log,
        TEXT("[Runtime] Steam 초기화 통과 (OSS=%s, SteamID=%llu)."),
        *SubsystemName.ToString(),
        static_cast<unsigned long long>(Id.ConvertToUint64()));
}

void USteamCredentialsSubsystem::RequestTicket(FCallback Callback)
{
    if (!bAvailable)
    {
        UE_LOG(LogSteamCredentials, Error, TEXT("RequestTicket 거부 — 서브시스템 비활성"));
        Callback(false, FString(), FString());
        return;
    }

    // self-managed — Start 안에서 모든 실패 경로가 Finish로 수렴해 delete this.
    auto* Req = new FTicketRequest(MoveTemp(Callback));
    Req->Start();
}
