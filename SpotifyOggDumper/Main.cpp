#include "pch.h"
#include <thread>
#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Hooks.h"
#include "CefUtils.h"

HMODULE _selfModule;
std::shared_ptr<StateManager> _stateMgr;

template <int... Offsets>
constexpr char* TraversePointers(void* ptr)
{
    for (int offset : { Offsets... }) {
        ptr = *(char**)((char*)ptr + offset);
    }
    return (char*)ptr;
}
std::string ToHex(char* data, int length)
{
    std::string str(length * 2, '\0');

    for (int i = 0; i < length; i++) {
        const char ALPHA[] = "0123456789abcdef";
        str[i * 2 + 0] = ALPHA[(data[i] >> 4) & 15];
        str[i * 2 + 1] = ALPHA[(data[i] >> 0) & 15];
    }
    return str;
}

DETOUR_FUNC(__fastcall, int, DecodeAudioData, (
    void* ecx, void* edx, int param_2, void** param_3, void** param_4, int param_5
))
{
    //PlayerDriver can be found at ecx in seek function
    //struct PlayerDriver { 
    //    0x330  uint8_t playback_id[16]
    //}
    //The decoder->player path can be found with CheatEngine:
    //1. Find the player address with the track seek function
    //2. Find the parent decoder address (in caller of this function)
    //3. Use the pointer scanner:
    //  1. Search for Addr: player address
    //  2. BaseAddr range: <decoder addr> .. <decoder addr> + 1000
    //If things go well, you should endup with less than 1k results.
    //It can be further refined by forcing the player addr to change
    //(by changing tracks?), or restarting spotify and matching the results
    //4. Pick one and see if it works with the goto address thing (cheatengine works, but x32dbg shows the result in real time)
    
    //009E1086 | mov ecx,dword ptr ss:[ebp-2C]  ; we need to access this
    //009E1089 | push eax                     
    //009E108A | lea eax,dword ptr ss:[ebp-50]
    //009E108D | push eax                     
    //009E108E | mov ecx,dword ptr ds:[ecx+38]
    //009E1091 | call spotify.9E08F1            ; call <the func this detour hooks>
    void* _ebp;
    __asm {
        mov _ebp, ebp
    }
    //caller `ebp-2C` = ebp + 90;   `(ebp - 2C) - esp`    before prolog's `mov ebp, esp`
    //path = [[[[ebp+90]+3C]+B8]+1E8]+150]+330

    auto buf = (char*)param_4[0];
    int bufLen = (int)param_4[1];

    int ret = DecodeAudioData_Orig(ecx, edx, param_2, param_3, param_4, param_5);

    int bytesRead = bufLen - (int)param_4[1];

    if (bytesRead != 0) {
        char* playerPtr = TraversePointers<0x90, 0x3C, 0xB8, 0x1E8, 0x150>(_ebp);
        std::string playbackIdStr = ToHex(playerPtr + 0x330, 16);
        _stateMgr->ReceiveAudioData(playbackIdStr, buf, bytesRead);
    }
    return ret;
}

DETOUR_FUNC(__fastcall, void*, CreateTrackPlayer, (
    void* ecx, void* edx, int param_1, int param_2, double speed,
    int param_4, int param_5, int param_6, int param_7, int param_8
))
{
    std::string playbackId = ToHex((char*)(param_2 + 8), 16);
    LogTrace("CreateTrack {}", playbackId);
    _stateMgr->OnTrackCreated(playbackId, speed);

    return CreateTrackPlayer_Orig(ecx, edx, param_1, param_2, speed, param_4, param_5, param_6, param_7, param_8);
}
DETOUR_FUNC(__fastcall, int64_t, SeekTrack, (
    void* ecx, void* edx, int64_t position
))
{
    auto playerPtr = TraversePointers<0x1740>(ecx);
    if (playerPtr) {
        std::string playbackId = ToHex(playerPtr + 0x330, 16);
        LogTrace("SeekTrack {}", playbackId);
        _stateMgr->DiscardTrack(playbackId, "Track was seeked");
    }
    return SeekTrack_Orig(ecx, edx, position);
}
DETOUR_FUNC(__fastcall, void, OpenTrack, (
    void* ecx, void* edx, int param_1, char* param_2, int* param_3,
    int64_t position, char param_5, int* param_6
))
{
    if (position != 0) {
        std::string playbackId = ToHex(param_2 + 0x330, 16);
        LogTrace("OpenTrack {}", playbackId);
        _stateMgr->DiscardTrack(playbackId, "Track didn't play from start");
    }
    OpenTrack_Orig(ecx, edx, param_1, param_2, param_3, position, param_5, param_6);
}
DETOUR_FUNC(__fastcall, void, CloseTrack, (
    void* ecx, void* edx, int param_1, void* param_2, char* reason, int param_4, char param_5
))
{
    auto playerPtr = TraversePointers<0x1740>(ecx);
    if (playerPtr) {
        std::string playbackId = ToHex(playerPtr + 0x330, 16);
        LogTrace("CloseTrack {}, reason={}", playbackId, reason);

        if (strcmp(reason, "trackdone") != 0) {
            _stateMgr->DiscardTrack(playbackId, "Track was skipped");
        }
        _stateMgr->OnTrackDone(playbackId);
    }
    CloseTrack_Orig(ecx, edx, param_1, param_2, reason, param_4, param_5);
}

void InstallHooks()
{
    //Signatures for Spotify v1.1.84.716-??
    CREATE_HOOK_PATTERN(DecodeAudioData,    "Spotify.exe", "55 8B EC 8B 11 56 8B 75 10 57 8B 7D 0C FF 75 14 8B 47 04 89 45 0C 8B 46 04 89 45 10 8D 45 10 50 FF 36 8D 45 0C 50 FF 37 FF 75 08 FF 52 04 8B 55 0C 8B CA 29 57 04 8B 45 08 C1 E1 02 01 0F 8B 4D 10 01 0E 29 4E 04 5F 5E 5D C2 10 00");
    CREATE_HOOK_PATTERN(CreateTrackPlayer,  "Spotify.exe", "68 9C 00 00 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 8D 60 FF FF FF 8B 45 18 33 DB 8B 7D 08 8B 75 0C F2 0F 10 45 10 89 85 6C FF FF FF 8B 45 1C 89 85 70 FF FF FF 8B 45 20 89 85 74 FF FF FF 8B 45 24 89 BD 68 FF FF FF 89 85 68 FF FF FF 8B 45 28 89 85 78 FF FF FF 8D 46 08 6A 10 50 8D 45 CC F2 0F 11 85 58 FF FF FF 50 89 9D 64 FF FF FF");
    CREATE_HOOK_PATTERN(SeekTrack,          "Spotify.exe", "55 8B EC 83 EC 38 A1 ?? ?? ?? ?? 33 C5 89 45 FC 56 8B F1 8B 8E 40 17 00 00 85 C9 75 04 32 C0 EB 5D 8B 01 8D 55 EC 52 8B 80 8C 00 00 00 FF D0 6A 10 50 8D 45 C8 50");
    CREATE_HOOK_PATTERN(OpenTrack,          "Spotify.exe", "68 54 01 00 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F1 89 B5 DC FE FF FF 8B 45 08 33 FF 89 85 BC FE FF FF 8B 45 10 89 85 B0 FE FF FF 8A 45 1C 88 85 C3 FE FF FF 89 BD E4 FE FF FF 89 BD E0 FE FF FF 8B 4D 0C 8D 55 E0 21 7D FC FF 86 ?? ?? 00 00 52 8B 01 8B 80 8C 00 00 00 FF D0 6A 10 50 8D 45 BC 50");
    CREATE_HOOK_PATTERN(CloseTrack,         "Spotify.exe", "6A 50 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F9 83 AF EC 17 00 00 01 8A 45 18 8B 55 0C 8B 5D 08 8B 75 10 89 55 AC 88 45 BB 74 11 6A 01 8B CB E8 ?? ?? ?? ?? 6A 02 58 E9 19 03 00 00 8B 8F 40 17 00 00 8D 55 E0 52 8B 01 FF 90 8C 00 00 00 6A 10 50 8D 45 BC 50");
    
    auto urlreqHook = CefUtils::InitUrlBlocker([&](auto url) { return _stateMgr && _stateMgr->IsUrlBlocked(url); });
    Hooks::CreateApi(L"libcef.dll", "cef_urlrequest_create", urlreqHook.first, urlreqHook.second);
    Hooks::EnableAll();
}

std::filesystem::path GetModuleFileNameEx(HMODULE module)
{
    //gotta hate those windows apis
    //https://stackoverflow.com/a/33613252
    std::vector<wchar_t> pathBuf;
    DWORD copied = 0;
    do {
        pathBuf.resize(pathBuf.size() + MAX_PATH);
        copied = GetModuleFileName(module, &pathBuf.at(0), pathBuf.size());
    } while (copied >= pathBuf.size());
    pathBuf.resize(copied);

    return std::filesystem::path(pathBuf.begin(), pathBuf.end());
}

std::string GetFileVersion(const std::wstring& fn)
{
    int versionDataLen = GetFileVersionInfoSize(fn.c_str(), NULL);
    auto versionData = std::make_unique<uint8_t[]>(versionDataLen);
    GetFileVersionInfo(fn.c_str(), 0, versionDataLen, versionData.get());

    VS_FIXEDFILEINFO* info;
    UINT infoLen;
    VerQueryValue(versionData.get(), L"\\", (LPVOID*)&info, &infoLen);

    return std::format(
        "{}.{}.{}.{}",
        HIWORD(info->dwFileVersionMS),
        LOWORD(info->dwFileVersionMS),
        HIWORD(info->dwFileVersionLS),
        LOWORD(info->dwFileVersionLS)
    );
}

void Exit()
{
    LogInfo("Uninstalling...");

    Hooks::DisableAll();

    _stateMgr->Shutdown();
    _stateMgr = nullptr;

    CloseLogger();
    FreeLibraryAndExitThread(_selfModule, 0);
}

DWORD WINAPI Init(LPVOID param)
{
    auto dataDir = GetModuleFileNameEx(_selfModule).parent_path();
    
    bool logToCon = true;
    fs::path logFile = dataDir / "log.txt";
#if NDEBUG
    logToCon = fs::exists(dataDir / "_debug.txt");
    LogMinLevel = logToCon ? LogLevel::TRACE : LogLevel::DEBUG;
#endif

    std::string spotifyVersion = "<unknown>";

    try {
        InitLogger(logToCon, logFile);
        
        _stateMgr = StateManager::New(dataDir);
    
        spotifyVersion = GetFileVersion(L"Spotify.exe");
        LogInfo("Spotify version: {}", spotifyVersion);

        InstallHooks();

        std::thread(&StateManager::RunControlServer, _stateMgr).detach();
    } catch (std::exception& ex) {
        auto msg = std::format(
            "Failed to initialize Soggfy: {}\n\n"
            "This likely means that the Spotify version you are using ({}) is not supported.\n"
            "Try updating Soggfy, or downgrading Spotify to the latest supported "
            "version linked in the readme.",
            ex.what(), spotifyVersion
        );
        LogError("{}", msg);
        MessageBoxA(NULL, msg.c_str(), "Soggfy", MB_ICONERROR);
        Exit();
    }
    LogInfo("Hooks were successfully installed.");

    if (logToCon) {
        while (true) {
            auto ch = std::tolower(std::cin.get());

            if (ch == 'l') {
                LogMinLevel = (LogLevel)(LogMinLevel == LOG_TRACE ? LOG_INFO : LogMinLevel - 1); // cycle through [INFO, DEBUG, TRACE]
                LogInfo("Min log level set to {}", (int)LogMinLevel);
            }
            if (ch == 'u') break;
        }
        Exit();
    }
    return 0;
}

#if _WINDLL

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        _selfModule = hModule;
        CreateThread(NULL, 0, Init, NULL, 0, NULL);
    }
    return TRUE;
}
#else

#include "ControlServer.h"

#include "Utils/Utils.h"

int main()
{
    LogMinLevel = LOG_TRACE;
    ControlServer sv([](auto con, auto&& msg) {
        LogInfo("Received message {}: {} (+{} bytes)", (int)msg.Type, msg.Content.dump(), msg.BinaryContent.size());
    });
    std::thread(&ControlServer::Run, &sv).detach();

    LogInfo("Press any key to exit");
    std::cin.get();
    sv.Stop();
    return 0;
}
#endif