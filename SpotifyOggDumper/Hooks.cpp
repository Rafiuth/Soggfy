#include "pch.h"
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

//Note: __thiscall can't be used directly; as a workaround, try __fastcall with an extra parameter for edx after 'this':
//__thiscall FN(void* this, int x)  ->  __fastcall FN(void* ecx, void* edx, int x)
#define DETOUR_FUNC(CALL_CONV, RET_TYPE, NAME,  SIG)            \
    typedef RET_TYPE (CALL_CONV *NAME##_FuncType)SIG;           \
    NAME##_FuncType NAME##_Org;                                 \
    RET_TYPE CALL_CONV NAME##_Detour##SIG

template <int... Offsets>
constexpr void* TraversePointers(void* ptr)
{
    for (int offset : { Offsets... }) {
        ptr = *(char**)((char*)ptr + offset);
    }
    return ptr;
}

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

    int ret = DecodeAudioData_Org(ecx, edx, param_2, param_3, param_4, param_5);

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

    WriteLog_Org(pars);
}
DETOUR_FUNC(__cdecl, int, CreateJsonAccessToken, (void* param_1, char** param_2))
{
    if (param_2) {
        auto accToken = *(char**)param_2;
        _stateMgr->UpdateAccToken(accToken);
    }
    return CreateJsonAccessToken_Org(param_1, param_2);
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
    CreateTrackPlayer_Org(ecx, edx, param_2, param_3, speed, param_5, param_6, param_7, param_8, param_9);
}


struct HookTableEntry
{
    uintptr_t Addr;
    LPVOID Detour;
    LPVOID* OrgFunc;
};
struct HookTable
{
    AppVersion Version;
    HookTableEntry Entries[16];
};
static const HookTable HookTables[] =
{
    {
        {1, 1, 70, 610},   //version
        {
            { 0x009E08F1, &DecodeAudioData_Detour,          (LPVOID*)&DecodeAudioData_Org       },
            { 0x00CC4953, &WriteLog_Detour,                 (LPVOID*)&WriteLog_Org              },
            { 0x008B10EC, &CreateJsonAccessToken_Detour,    (LPVOID*)&CreateJsonAccessToken_Org },
            { 0x009A065B, &CreateTrackPlayer_Detour,        (LPVOID*)&CreateTrackPlayer_Org     },
        }
    }
};

void InstallHooks(const HookTable& table)
{
    MH_Initialize();

    for (auto& entry : table.Entries) {
        if (entry.Addr == 0) break;
        if (MH_CreateHook((LPVOID)entry.Addr, entry.Detour, entry.OrgFunc) != MH_OK) {
            throw std::runtime_error("Failed to create hook for " + std::to_string(entry.Addr));
        }
    }
    auto enableResult = MH_EnableHook(MH_ALL_HOOKS);
    if (enableResult != MH_OK) {
        throw std::runtime_error("Failed to enable hooks (code " + std::to_string(enableResult) + ")");
    }
}
void InstallHooks(const AppVersion& version)
{
    for (auto& table : HookTables) {
        if (version == table.Version) {
            InstallHooks(table);
            return;
        }
    }
    throw std::runtime_error(std::format("No hook table available for version {}", version));
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
        R"(                                                        v1.4.4)";
    LogInfo(label);

    try {
        auto spotifyVersion = AppVersion::Of(L"Spotify.exe");
        LogInfo("Spotify version: {}", spotifyVersion);

        InstallHooks(spotifyVersion);
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
