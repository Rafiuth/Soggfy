#include "Hooks.h"
#include <mutex>
#include <deque>
#include <MinHook.h>
#include "AppVersion.h"
#include "StateManager.h"
#include "Utils/Log.h"

HMODULE _selfModule;
std::deque<std::string> _logHistory;
std::shared_ptr<StateManager> _stateMgr;

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
    //    0x340  uint8_t playback_id[16]
    //}
    //[[[ecx+3c]+294]+150]+0
    //The decoder->driver path can be found with CheatEngine:
    //1. Find the driver address with the track seek function
    //2. Find the parent decoder address (in caller of this function)
    //3. Use the pointer scanner:
    //  1. Search for Addr: driver address
    //  2. BaseAddr range: <decoder addr> .. <decoder addr> + 1000
    //If things go well, you should endup with less than 1k results.
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
    //path = [[[[ebp+90]+3c]+294]+150]+0
    char* playerPtr = TraversePointers<0x90, 0x3C, 0x294, 0x150>(_ebp);
    std::string playbackIdStr = ToHex(playerPtr + 0x340, 16);

    auto buf = (char*)param_4[0];
    int bufLen = (int)param_4[1];

    int ret = DecodeAudioData_Orig(ecx, edx, param_2, param_3, param_4, param_5);

    int bytesRead = bufLen - (int)param_4[1];

    if (bytesRead != 0) {
        _stateMgr->ReceiveAudioData(playbackIdStr, buf, bytesRead);
    }
    return ret;
}

DETOUR_FUNC(__fastcall, void, CreateTrackPlayer, (
    void* ecx, void* edx, int param_2, int param_3, double speed, 
    int param_5, int param_6, int param_7, int param_8, int param_9
))
{
    _stateMgr->OverridePlaybackSpeed(speed);
    CreateTrackPlayer_Orig(ecx, edx, param_2, param_3, speed, param_5, param_6, param_7, param_8, param_9);
}
DETOUR_FUNC(__fastcall, int64_t, SeekTrack, (
    void* ecx, void* edx, int64_t position
))
{
    auto playerPtr = TraversePointers<0x15B4>(ecx);
    if (playerPtr) {
        std::string playbackId = ToHex(playerPtr + 0x340, 16);
        LogDebug("SeekTrack playId={} position={}", playbackId, position);
        _stateMgr->DiscardTrack(playbackId);
    }
    return SeekTrack_Orig(ecx, edx, position);
}
DETOUR_FUNC(__fastcall, void, CloseTrack, (
    void* ecx, void* edx, int param_2, int param_3, char* reason, int param_5, char param_6
))
{
    auto playerPtr = TraversePointers<0x15B4>(ecx);
    if (playerPtr) {
        std::string playbackId = ToHex(playerPtr + 0x340, 16);
        LogDebug("CloseTrack playId={} reason={}", playbackId, reason);

        if (strcmp(reason, "trackdone") != 0) {
            _stateMgr->DiscardTrack(playbackId);
        }
        _stateMgr->OnTrackDone(playbackId);
    }
    CloseTrack_Orig(ecx, edx, param_2, param_3, reason, param_5, param_6);
}

static const HookInfo HookTargets[] =
{
    HOOK_INFO(
        DecodeAudioData,
        Fingerprint(
            L"Spotify.exe",
            "\x55\x8B\xEC\x8B\x11\x56\x8B\x75\x10\x57\x8B\x7D\x0C\xFF\x75\x14\x8B\x47\x04\x89\x45\x0C\x8B\x46\x04\x89\x45\x10\x8D\x45\x10\x50\xFF\x36\x8D\x45\x0C\x50\xFF\x37\xFF\x75\x08\xFF\x52\x04\x8B\x55\x0C\x8B\xCA\x29\x57\x04\x8B\x45\x08\xC1\xE1\x02\x01\x0F\x8B\x4D\x10\x01\x0E\x29\x4E\x04\x5F\x5E\x5D\xC2\x10\x00",
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
        )
    ),
    HOOK_INFO(
        CreateTrackPlayer,
        Fingerprint(
            L"Spotify.exe",
            "\x6A\x74\xB8\x52\xBC\xD9\x00\xE8\x54\x67\x34\x00\x89\x4D\x88\x8B\x45\x18\x33\xDB\x8B\x7D\x08\x8B\x75\x0C\xF2\x0F\x10\x45\x10\x89\x45\x94\x8B\x45\x1C\x89\x45\x98\x8B\x45\x20\x89\x45\x9C\x8B\x45\x24\x89\x7D\x90\x89\x45\x90\x8B\x45\x28\x89\x45\xA0\x8D\x46\x08\x6A\x10\x50\x8D\x45\xCC\xF2\x0F\x11\x45\x80\x50\x89\x5D\x8C\xE8\x1A\x8E\x32\x00\x8D\x45\xCC\x50\x68\x30\xF5\xE8\x00\x53\x68\x19\x01\x00\x00\x68\x70\xC8\xDE\x00\x68\x7A\x42\xDE\x00\x53\x6A\x04\xE8\x83\x42\x32\x00",
            "\xFF\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
        )
    ),
    HOOK_INFO(
        SeekTrack,
        Fingerprint(
            L"Spotify.exe",
            "\x55\x8B\xEC\x83\xEC\x38\xA1\x18\xB0\x06\x01\x33\xC5\x89\x45\xFC\x56\x8B\xF1\x8B\x8E\xB4\x15\x00\x00\x85\xC9\x75\x04\x32\xC0\xEB\x5D\x8B\x01\x8D\x55\xEC\x52\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x6A\x10\x50\x8D\x45\xC8\x50\xE8\x4C\x6A\x32\x00\x8D\x45\xC8\x50\xFF\x75\x0C\xFF\x75\x08\x68",
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
        )
    ),
    HOOK_INFO(
        CloseTrack,
        Fingerprint(
            L"Spotify.exe",
            "\x6A\x50\xB8\x05\xBC\xD9\x00\xE8\xE0\x6A\x34\x00\x8B\xF9\x83\xAF\x5C\x16\x00\x00\x01\x8A\x45\x18\x8B\x55\x0C\x8B\x5D\x08\x8B\x75\x10\x89\x55\xAC\x88\x45\xBB\x74\x23\x8A\x0D\xBC\xF3\xE8\x00\x8D\x45\xA8\x88\x4D\xAC\x8B\xCB\x50\xFF\x75\xAC\xC7\x45\xA8\x01\x00\x00\x00\xE8\x84\xEA\xFF\xFF\xE9\x1E\x03\x00\x00\x8B\x8F\xB4\x15\x00\x00\x8D\x55\xE0\x52\x8B\x01\xFF\x90\x8C\x00\x00\x00\x6A\x10\x50\x8D\x45\xBC\x50\xE8\x90\x91\x32\x00\x8D\x45\xBC\x50\x68\x58\xF7\xE8\x00\x6A\x00\x68\xA6\x02\x00\x00\x68\x70\xC8\xDE\x00\x68\x7A\x42\xDE\x00\x6A\x00\x6A\x04\xE8\xF7\x45\x32\x00",
            "\xFF\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
        )
    )
};

void InstallHooks()
{
    MH_Initialize();

    for (auto& hook : HookTargets) {
        uintptr_t addr = hook.Fingerprint.SearchInModule();

        if (MH_CreateHook((LPVOID)addr, hook.Detour, hook.OrigFunc) != MH_OK) {
            throw std::runtime_error("Failed to create hook");
        }
    }
    auto enableResult = MH_EnableHook(MH_ALL_HOOKS);
    if (enableResult != MH_OK) {
        throw std::runtime_error("Failed to enable hooks (code " + std::to_string(enableResult) + ")");
    }
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

void Exit()
{
    LogInfo("Uninstalling... (you can close this console now)");

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    _stateMgr->Shutdown();
    _stateMgr = nullptr;

    FreeConsole();
    FreeLibraryAndExitThread(_selfModule, 0);
}

DWORD WINAPI Init(LPVOID param)
{
    InstallConsole();

    auto dataDir = GetModuleFileNameEx(_selfModule).parent_path();
    _stateMgr = StateManager::New(dataDir);

    try {
        auto spotifyVersion = AppVersion::Of(L"Spotify.exe");
        LogInfo("Spotify version: {}", spotifyVersion.AsString());

        InstallHooks();

        std::thread(&StateManager::RunControlServer, _stateMgr).detach();
    } catch (std::exception& ex) {
        LogError("Failed to install hooks: {}", ex.what());
        Exit();
    }
    LogInfo("Hooks were successfully installed.");

    //TODO: eject through UI?
    while (true) {
        auto ch = std::tolower(std::cin.get());

        if (ch == 'l') {
            LogMinLevel = (LogLevel)(LogMinLevel == LOG_TRACE ? LOG_INFO : LogMinLevel - 1); // cycle through [INFO, DEBUG, TRACE]
            LogInfo("Min log level set to {}", (int)LogMinLevel);
        }
        if (ch == 'u') break;
    }
    Exit();
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

int main()
{
    LogMinLevel = LOG_TRACE;
    ControlServer sv([](auto con, auto& msg) {
        LogInfo("Received message {}: {} (+{} bytes)", (int)msg.Type, msg.Content.dump(), msg.BinaryContent.size());
    });
    std::thread(&ControlServer::Run, &sv).detach();

    LogInfo("Press any key to exit");
    std::cin.get();
    sv.Stop();
    return 0;
}
#endif