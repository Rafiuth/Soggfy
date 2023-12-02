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
std::string ToHex(const uint8_t* data, int length)
{
    std::string str(length * 2, '\0');

    for (int i = 0; i < length; i++) {
        const char ALPHA[] = "0123456789abcdef";
        str[i * 2 + 0] = ALPHA[(data[i] >> 4) & 15];
        str[i * 2 + 1] = ALPHA[(data[i] >> 0) & 15];
    }
    return str;
}

struct PlayerState {
    uint8_t unknown1[0x3E0];
    uint8_t playback_id[16];

    std::string getPlaybackId() const {
        return ToHex(playback_id, 16);
    }
};
struct PlayerDriver {
    uint8_t unknown1[0x18C4];
    PlayerState* state;
};

DETOUR_FUNC(__fastcall, int, DecodeAudioData, (
    void* ecx, void* edx, int param_2, void** param_3, void** param_4, int param_5
))
{
    //This function is our main target. It takes a buffer containing an audio packet compressed in some particular
    //format (naturally OGG + Vorbis, but could be AAC or MP3), and calls a virtual function that decodes it into
    //a raw PCM sample buffer.
    //
    //Finding this function is somewhat difficult because it has no distinct search features. It's easier to find the
    //main OGG parser function (look for "OggS", 4) and then search for this dispatcher in the call stack with x64dbg.
    //
    //Associating the data flowing through here with individual songs is the major challange. Soggfy does this by linking
    //them via the PlaybackId field, which is an unique random 16-byte ID assigned to each playback by the Spotify client.
    //This ID is easily accessible from every other player control function, but a bit tricker to get from here.
    //
    //
    //Assuming that `Player` is a struct containing the PlaybackId field, and `Decoder` is an object accessible by
    //the caller function, the pointer path from `Decoder` to `Player` can be found using CheatEngine, if one exists:
    //
    //1. Find the player address by placing a breakpoint in the SeekTrack() function at `mov ecx, [esi+18C4]` (copy esi value)
    //2. Find the decoder address by placing a breakpoint in the caller of DecodeAudioData(), just before `mov ecx, [ecx + 3C]`
    //3. Use the pointer scanner:
    //  1. Search for Addr: <player address>
    //  2. Base addr in specific range: <decoder addr> .. <decoder addr> + 1000
    //4. Restart Spotify and repeat 1 and 2, then use "Rescan Memory" to filter out dead paths
    //5. Pick one of the results and see if it works with the "goto address" thing (x32dbg is better for this)
    //     Note: First offset is the addr shown in the first column subtracted by the base decoder addr.
    //
    //Caller assembly as of v1.2.25.1011:
    //    mov ecx,dword ptr ss:[ebp-40]     ; ecx = [decoder ptr]   <-- we need this value
    //    push eax                      
    //    lea eax,dword ptr ss:[ebp-64] 
    //    push eax                      
    //    mov ecx,dword ptr ds:[ecx+3C]     ; ecx = ecx->field_3C
    //    call spotify.6550D2               ; DecodeAudioData(ecx, edx, <stack>)
    //
    //After DecodeAudioData's prolog (push ebp; mov ebp, esp), [ebp] will contain the value of ebp in the caller function, thus:
    //  Path found in CE: [[[[ecx+40]+128]+1E8]+150]
    //  From this function: [[[[[[ebp]-40]+40]+128]+1E8]+150]
    void* _ebp;
    __asm { mov _ebp, ebp }

    auto buf = (char*)param_4[0];
    int bufLen = (int)param_4[1];

    int ret = DecodeAudioData_Orig(ecx, edx, param_2, param_3, param_4, param_5);

    int bytesRead = bufLen - (int)param_4[1];

    if (bytesRead != 0) {
        auto playerState = (PlayerState*)TraversePointers<0, -0x40, 0x40, 0x128, 0x1E8, 0x150>(_ebp);
        std::string playbackId = playerState->getPlaybackId();
        _stateMgr->ReceiveAudioData(playbackId, buf, bytesRead);
    }
}

DETOUR_FUNC(__fastcall, void*, CreateTrackPlayer, (
    void* ecx, void* edx, int param_1, int param_2, double speed,
    int param_4, int param_5, int param_6, int param_7, int param_8
))
{
    std::string playbackId = ToHex((uint8_t*)(param_2 + 0x58), 16);
    LogTrace("CreateTrack {}", playbackId);
    _stateMgr->OnTrackCreated(playbackId, speed);

    return CreateTrackPlayer_Orig(ecx, edx, param_1, param_2, speed, param_4, param_5, param_6, param_7, param_8);
}
DETOUR_FUNC(__fastcall, int64_t, SeekTrack, (
    PlayerDriver* ecx, void* edx, int64_t position
))
{
    if (ecx->state) {
        std::string playbackId = ecx->state->getPlaybackId();
        LogTrace("SeekTrack {}", playbackId);
        _stateMgr->DiscardTrack(playbackId, "Track was seeked");
    }
    return SeekTrack_Orig(ecx, edx, position);
}
DETOUR_FUNC(__fastcall, void, OpenTrack, (
    void* ecx, void* edx, int param_1, PlayerState* param_2, int* param_3,
    int64_t position, char param_5, int* param_6
))
{
    if (position != 0) {
        std::string playbackId = param_2->getPlaybackId();
        LogTrace("OpenTrack {}", playbackId);
        _stateMgr->DiscardTrack(playbackId, "Track didn't play from start");
    }
    OpenTrack_Orig(ecx, edx, param_1, param_2, param_3, position, param_5, param_6);
}
DETOUR_FUNC(__fastcall, void, CloseTrack, (
    PlayerDriver* ecx, void* edx, int param_1, void* param_2, char* reason, int param_4, char param_5
))
{
    if (ecx->state) {
        std::string playbackId = ecx->state->getPlaybackId();
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
    //Signatures for Spotify v1.2.25
    CREATE_HOOK_PATTERN(DecodeAudioData,    "Spotify.exe", "55 8B EC 51 56 8B 75 0C 8D 55 0C 57 FF 75 14 8B 7D 10 8B 46 04 52 8D 55 FC 89 45 FC FF 37 8B 47 04 52 FF 36 89 45 0C 8B 01 FF 75 08 FF 50 04 8B 06 8B 4D FC 29 4E 04 8D 14 88 8B 45 08 89 16 8B 17 8B 4F 04 03 55 0C 2B 4D 0C 89 17 89 4F 04 5F 5E C9 C2 10 00");
    CREATE_HOOK_PATTERN(CreateTrackPlayer,  "Spotify.exe", "6A 70 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 4D A0 8B 45 08 8B 4D 0C F2 0F 10 45 10 8B 5D 20 8B 7D 24 8B 75 28 89 45 98 89 45 90 8B 45 18 89 45 98 8B 45 1C 89 45 94 8D 41 58 6A 10 50 8D 45 CC 89 4D 9C 50 F2 0F 11 45 88 E8 ?? ?? ?? ?? 8D 45 CC 50 68 ?? ?? ?? ??");
    CREATE_HOOK_PATTERN(SeekTrack,          "Spotify.exe", "68 A0 00 00 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F1 8B 8E C4 18 00 00 85 C9 75 07 32 C0 E9 A0 01 00 00 8B 01 8D 55 C0 52 8B 80 8C 00 00 00 FF D0 6A 10 50 8D 45 9C 50 E8 ?? ?? ?? ?? 8D 45 9C 50 FF 75 0C FF 75 08 68 ?? ?? ?? ??");
    CREATE_HOOK_PATTERN(OpenTrack,          "Spotify.exe", "68 FC 02 00 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F1 89 B5 14 FE FF FF 8B 45 10 8B 7D 08 83 A5 08 FE FF FF 00 89 85 00 FE FF FF 8A 45 1C 89 BD FC FD FF FF 88 85 07 FE FF FF 8B 4D 0C 8D 55 DC 83 65 FC 00 FF 86 48 19 00 00 52 8B 01 8B 80 8C 00 00 00 FF D0 6A 10 50 8D 45 B8 50 E8 ?? ?? ?? ?? 8D 45 B8 50 68 ?? ?? ?? ??");
    CREATE_HOOK_PATTERN(CloseTrack,         "Spotify.exe", "68 D8 00 00 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B D9 83 AB 48 19 00 00 01 8B 45 0C 8B 75 08 8B 7D 10 89 85 64 FF FF FF 8B 45 14 89 85 78 FF FF FF 8A 45 18 89 B5 60 FF FF FF 89 BD 68 FF FF FF 88 45 83 74 27 6A 01 8D 8D 5C FF FF FF E8 3B E4 FF FF 8B 8D 5C FF FF FF 8B 95 60 FF FF FF 89 0E 89 56 04 C6 46 08 00 E9 ?? ?? ?? ?? 8B 8B C4 18 00 00");
    
    auto urlreqHook = CefUtils::InitUrlBlocker([&](auto url) { return _stateMgr && _stateMgr->IsUrlBlocked(url); });
    Hooks::CreateApi(L"libcef.dll", "cef_urlrequest_create", urlreqHook.first, urlreqHook.second);
    Hooks::EnableAll();
}

std::filesystem::path GetModuleFileNameEx(HMODULE module)
{
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
    LogMinLevel = logToCon ? LOG_TRACE : LOG_DEBUG;
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