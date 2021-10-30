#include "Hooks.h"
#include <mutex>
#include <deque>
#include <MinHook.h>
#include "AppVersion.h"
#include "StateManager.h"
#include "Utils/Log.h"

HMODULE _selfModule;
std::deque<std::string> _logHistory;
std::unique_ptr<StateManager> _stateMgr;
std::mutex _mutex;

double _playbackSpeed = -1.0;

/*
undefined4 __thiscall
FUN_009e08f1(void *this,undefined4 param_2,void **param_3,void **param_4, undefined4 param_5)
*/
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
    void* playerPtr = TraversePointers<0x90, 0x3C, 0x294, 0x150>(_ebp);
    auto playbackId = (uint8_t*)playerPtr + 0x340;

    std::string playbackIdStr(32, '\0');
    for (int i = 0; i < 16; i++) {
        constexpr auto ALPHA = "0123456789abcdef";
        playbackIdStr[i * 2 + 0] = ALPHA[playbackId[i] >> 4];
        playbackIdStr[i * 2 + 1] = ALPHA[playbackId[i] & 15];
    }
    auto buf = (char*)param_4[0];
    int bufLen = (int)param_4[1];

    int ret = DecodeAudioData_Orig(ecx, edx, param_2, param_3, param_4, param_5);

    int bytesRead = bufLen - (int)param_4[1];

    if (bytesRead != 0) {
        _mutex.lock();
        _stateMgr->ReceiveAudioData(playbackIdStr, buf, bytesRead);
        _mutex.unlock();
    }
    return ret;
}

//The log function uses varargs, we use a fixed array to forward the arguments.
//It works because call arguments are pushed from right to left on the stack,
//so it doesn't matter if we push more than the actual number of args.
struct LogParams
{
    int param_1;    //log level?
    int param_2;
    int param_3;    //global address
    int param_4;    //global address
    int param_5;    //some sequential id unique per format
    int param_6;
    const char* format;
    void* args[32];
};

DETOUR_FUNC(__cdecl, void, WriteLog, (LogParams pars))
{
    char buf[1024];
    va_list vl;
    va_start(vl, pars.format);
    vsprintf_s(buf, pars.format, vl);
    va_end(vl);

    std::string str(buf);
    LogTrace("* " + str);

    _mutex.lock();

    //Keep history of at most 16 messages
    if (_logHistory.size() > 16) {
        _logHistory.pop_back();
    }
    _logHistory.push_front(str);

    std::smatch match;
    auto MatchesRegex = [&](const std::string& str, const char* pattern) -> bool {
        return std::regex_match(str, match, std::regex(pattern));
    };

    if (MatchesRegex(str, R"(\s*track_uri: (.+))")) {
        auto trackId = match.str(1);

        if (MatchesRegex(_logHistory.at(1), R"(\s*Creating track player for track \(playback_id (.+?)\))")) {
            auto playbackId = match.str(1);
            _stateMgr->OnTrackCreated(playbackId, trackId);
        } else {
            LogWarn("Found track uri with no matching playback");
        }
    }
    if (MatchesRegex(str, R"(\s*reason_end: (.+))")) {
        auto reason = match.str(1);

        if (MatchesRegex(_logHistory.at(2), R"(Close track \(playback_id (.+?)\))")) {
            auto playbackId = match.str(1);
            _stateMgr->OnTrackClosed(playbackId, reason);
        } else {
            LogWarn("Track closed with no matching playback");
        }
    }
    if (MatchesRegex(str, R"(\s*position: (-?\d+) ms)")) {
        int position = std::stoi(match.str(1));

        if (MatchesRegex(_logHistory.at(1), R"(\s*Open track player \(playback_id (.+?)\))")) {
            auto playbackId = match.str(1);
            _stateMgr->OnTrackOpened(playbackId, position);
        } else {
            LogWarn("Track opened with no matching playback");
        }
    }
    if (MatchesRegex(str, R"(Seeking track to \d+ ms \(playback_id (.+?)\))")) {
        _stateMgr->OnTrackSeeked(match.str(1));
    }
    _mutex.unlock();

    WriteLog_Orig(pars);
}
//void FUN_008b41bf(undefined4 param_1,undefined4 param_2,int *param_3)
DETOUR_FUNC(__cdecl, int, CreateJsonAccessToken, (void* param_1, void* param_2, void* param_3))
{
    if (param_2) {
        auto accToken = *(char**)param_2;
        _stateMgr->UpdateAccToken(accToken);
    }
    return CreateJsonAccessToken_Orig(param_1, param_2, param_3);
}

/* 
void __thiscall
FUN_009a065b(void *this,undefined4 param_2,int **param_3,double speed, undefined4 param_5,
            undefined4 param_6,undefined4 param_7,undefined4 param_8,undefined4 param_9)
*/
DETOUR_FUNC(__fastcall, void, CreateTrackPlayer, (
    void* ecx, void* edx, int param_2, int param_3, double speed, 
    int param_5, int param_6, int param_7, int param_8, int param_9
))
{
    if (_playbackSpeed > 0.0) {
        speed = _playbackSpeed;
    }
    CreateTrackPlayer_Orig(ecx, edx, param_2, param_3, speed, param_5, param_6, param_7, param_8, param_9);
}

static const HookInfo HookTargets[] =
{
    {
        &DecodeAudioData_Detour, (LPVOID*)&DecodeAudioData_Orig,
        Fingerprint(
            L"Spotify.exe",
            "\x55\x8B\xEC\x8B\x11\x56\x8B\x75\x10\x57\x8B\x7D\x0C\xFF\x75\x14\x8B\x47\x04\x89\x45\x0C\x8B\x46\x04\x89\x45\x10\x8D\x45\x10\x50\xFF\x36\x8D\x45\x0C\x50\xFF\x37\xFF\x75\x08\xFF\x52\x04\x8B\x55\x0C\x8B\xCA\x29\x57\x04\x8B\x45\x08\xC1\xE1\x02\x01\x0F\x8B\x4D\x10\x01\x0E\x29\x4E\x04\x5F\x5E\x5D\xC2\x10\x00",
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
        )
    },
    {
        &WriteLog_Detour, (LPVOID*)&WriteLog_Orig,
        Fingerprint(
            L"Spotify.exe",
            "\x6A\x24\xB8\xFE\x2E\xD1\x00\xE8\x2E\x26\x02\x00\x8B\x7D\x10\x33\xC0\x8B\x75\x14\x89\x45\xD8\x89\x45\xE8\xC7\x45\xEC\x0F\x00\x00\x00\x88\x45\xD8\x89\x45\xFC\x8D\x45\x24\x50\xFF\x75\x20\x8D\x45\xD8\x50\xE8\x6D\x2E\x00\x00\x8D\x45\xD8\x50\xFF\x75\x1C\xFF\x75\x18\x56\x57\xFF\x75\x0C\xFF\x75\x08\xE8\x40\x00\x00\x00\x8B\x45\xEC\x83\xC4\x28\x83\xF8\x10\x72\x2F\x8B\x4D\xD8\x40\x89\x45\xD4\x89\x4D\xD0\x3D\x00\x10\x00\x00\x72\x15\x8D\x45\xD4\x50\x8D\x45\xD0\x50\xE8\x94\x31\x7C\xFF",
            "\xFF\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
        )
    },
    {
        &CreateJsonAccessToken_Detour, (LPVOID*)&CreateJsonAccessToken_Orig,
        Fingerprint(
            L"Spotify.exe",
            "\x55\x8B\xEC\x6A\xFF\x68\xBB\x8E\xD8\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC\xA4\x00\x00\x00\xA1\x18\x00\x07\x01\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\x45\x08\x8B\x4D\x0C\x89\x45\x98\x8B\x45\x10\x51\x8D\x8D\x50\xFF\xFF\xFF\x8B\x38\x8B\x70\x04\xE8\xB7\x40\xC5\xFF\x89\x7D\x90\x89\x75\x94\x8B\x45\x98\x83\x65\xFC\x00\x8B\x30\xC6\x45\xFC\x01\x85\xFF\x75\x30\x80\x7D\x88\x00\x74\x2A\x8D\x85\x50\xFF\xFF\xFF\x50\x8D\x45\xD8\x50\xE8\x11\xFF\xFF\xFF",
            "\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
        )
    },
    {
        &CreateTrackPlayer_Detour, (LPVOID*)&CreateTrackPlayer_Orig,
        Fingerprint(
            L"Spotify.exe",
            "\x6A\x74\xB8\x52\xBC\xD9\x00\xE8\x54\x67\x34\x00\x89\x4D\x88\x8B\x45\x18\x33\xDB\x8B\x7D\x08\x8B\x75\x0C\xF2\x0F\x10\x45\x10\x89\x45\x94\x8B\x45\x1C\x89\x45\x98\x8B\x45\x20\x89\x45\x9C\x8B\x45\x24\x89\x7D\x90\x89\x45\x90\x8B\x45\x28\x89\x45\xA0\x8D\x46\x08\x6A\x10\x50\x8D\x45\xCC\xF2\x0F\x11\x45\x80\x50\x89\x5D\x8C\xE8\x1A\x8E\x32\x00\x8D\x45\xCC\x50\x68\x30\xF5\xE8\x00\x53\x68\x19\x01\x00\x00\x68\x70\xC8\xDE\x00\x68\x7A\x42\xDE\x00\x53\x6A\x04\xE8\x83\x42\x32\x00",
            "\xFF\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00"
        )
    }
};

void InstallHooks()
{
    MH_Initialize();

    for (auto& hook : HookTargets) {
        std::vector<LPVOID> addrs;
        hook.TargetFingerprint.SearchInModule(addrs);

        if (addrs.size() != 1) {
            throw std::runtime_error("Could not find address of hook target");
        }
        if (MH_CreateHook(addrs[0], hook.Detour, hook.OrigFunc) != MH_OK) {
            throw std::runtime_error("Failed to create hook");
        }
    }
    auto enableResult = MH_EnableHook(MH_ALL_HOOKS);
    if (enableResult != MH_OK) {
        throw std::runtime_error("Failed to enable hooks (code " + std::to_string(enableResult) + ")");
    }
}

void Exit()
{
    LogInfo("Uninstalling... (you can close this console now)");

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    _stateMgr->Shutdown();

    FreeConsole();
    FreeLibraryAndExitThread(_selfModule, 0);
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

DWORD WINAPI Init(LPVOID param)
{
    InstallConsole();

    auto dataDir = GetModuleFileNameEx(_selfModule).parent_path();
    _stateMgr = StateManager::New(dataDir);

    //greet with a cheap ascii art
    std::string label =
        COL_DARK_BLUE
        R"(  ________  ________  ________  ________  ________ ___    ___ )" "\n"
        R"( |\   ____\|\   __  \|\   ____\|\   ____\|\  _____\\  \  /  /|)" "\n"
        R"( \ \  \___|\ \  \|\  \ \  \___|\ \  \___|\ \  \__/\ \  \/  / /)" "\n"
        R"(  \ \_____  \ \  \\\  \ \  \  __\ \  \  __\ \   __\\ \    / / )" "\n"
        R"(   \|____|\  \ \  \\\  \ \  \|\  \ \  \|\  \ \  \_| \/  /  /  )" "\n"
        R"(     ____\_\  \ \_______\ \_______\ \_______\ \__\__/  / /    )" "\n"
        R"(    |\_________\|_______|\|_______|\|_______|\|__|\___/ /     )" "\n"
        R"(    \|_________|                                 \|___|/      )" "\n" COL_RESET
        R"(                                                        v1.4.6)";
    LogInfo(label);

    try {
        auto spotifyVersion = AppVersion::Of(L"Spotify.exe");
        LogInfo("Spotify version: {}", spotifyVersion);

        InstallHooks();
    } catch (std::exception& ex) {
        LogError("Failed to install hooks: {}", ex.what());
        Exit();
    }

    LogInfo("Hooks were successfully installed.");
    LogInfo(COL_GRAY "Commands: [u]ninstall | playback [s]peed <value> | [l]og level; [enter]");
    
    while (true) {
        auto ch = std::tolower(std::cin.get());
        
        if (ch == 's') {
            double newSpeed;
            if (std::cin >> newSpeed) {
                _playbackSpeed = newSpeed;
                LogInfo("Following tracks will be played at {}x speed.", newSpeed);
            } else {
                std::cin.clear();
                LogError("Example usage: `s 4`");
            }
        }
        if (ch == 'l') {
            LogMinLevel = (LogLevel)(LogMinLevel == LOG_TRACE ? LOG_INFO : LogMinLevel - 1); // cycle through [INFO, DEBUG, TRACE]
            LogInfo("Min log level set to {}", (int)LogMinLevel);
        }
        if (ch == 'u') break;
    }
    Exit();
    return 0;
}

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
