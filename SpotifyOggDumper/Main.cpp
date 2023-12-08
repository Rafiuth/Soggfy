#include "pch.h"
#include <thread>
#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Hooks.h"
#include "Utils/Utils.h"
#include "CefUtils.h"

HMODULE _selfModule;
std::shared_ptr<StateManager> _stateMgr;

struct PlayerState {
    uint8_t unknown1[0x3E0];
    uint8_t playback_id[16];

    std::string getPlaybackId() const {
        return Utils::ToHex(playback_id, 16);
    }
};

template<typename T>
struct AudioSpan {
    T* data;
    int size;
};

DETOUR_FUNC(__fastcall, int, DecodeAudioData, (
    void* ecx, void* edx, int param_2, AudioSpan<float>* param_3, AudioSpan<char>* param_4, int param_5
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

    auto encodedBuffer = *param_4;
    auto sampleBuffer = *param_3;

    int ret = DecodeAudioData_Orig(ecx, edx, param_2, param_3, param_4, param_5);

    int bytesRead = encodedBuffer.size - param_4->size;
    int samplesDecoded = sampleBuffer.size - param_3->size;

    if (bytesRead > 0) {
        auto playerState = (PlayerState*)Utils::TraversePointers<0, -0x40, 0x40, 0x128, 0x1E8, 0x150>(_ebp);
        std::string playbackId = playerState->getPlaybackId();
        _stateMgr->ReceiveAudioData(playbackId, encodedBuffer.data, bytesRead);
    }
    
    double playSpeed = _stateMgr->GetPlaySpeed();

    if (samplesDecoded > 0 && playSpeed > 1.0) {
        int samplesKeept = std::max(1, (int)(samplesDecoded / playSpeed));
        param_3->size = sampleBuffer.size - samplesKeept;
        param_3->data = sampleBuffer.data + samplesKeept;
    }

    return ret;
}

void InstallHooks()
{
    //Signatures for Spotify v1.2.25+
    CREATE_HOOK_PATTERN(DecodeAudioData,    "Spotify.exe", "55 8B EC 51 56 8B 75 0C 8D 55 0C 57 FF 75 14 8B 7D 10 8B 46 04 52 8D 55 FC 89 45 FC FF 37 8B 47 04 52 FF 36 89 45 0C 8B 01 FF 75 08 FF 50 04 8B 06 8B 4D FC 29 4E 04 8D 14 88 8B 45 08 89 16 8B 17 8B 4F 04 03 55 0C 2B 4D 0C 89 17 89 4F 04 5F 5E C9 C2 10 00");

    CefUtils::InitUrlBlocker([&](auto url) { return _stateMgr && _stateMgr->IsUrlBlocked(url); });
    Hooks::EnableAll();
}

std::filesystem::path GetModulePath(HMODULE module)
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
    auto dataDir = Utils::GetLocalAppDataFolder() / "Soggfy";
    auto moduleDir = GetModulePath(_selfModule).parent_path();
    
    if (!fs::exists(dataDir)) {
        fs::create_directories(dataDir);
    }
    
    bool logToCon = true;
    fs::path logFile = dataDir / "log.txt";
#if NDEBUG
    logToCon = fs::exists(dataDir / "_debug.txt");
    LogMinLevel = logToCon ? LOG_TRACE : LOG_DEBUG;
#endif

    std::string spotifyVersion = "<unknown>";

    try {
        InitLogger(logToCon, logFile);
        
        _stateMgr = StateManager::New(dataDir, moduleDir);
    
        spotifyVersion = GetFileVersion(L"Spotify.exe");
        LogInfo("Spotify version: {}", spotifyVersion);

        InstallHooks();

        std::thread(&StateManager::RunControlServer, _stateMgr).detach();
    } catch (std::exception& ex) {
        auto msg = std::format(
            "Failed to initialize Soggfy: {}\n\n"
            "This likely means that the Spotify version you are using ({}) is not supported.\n"
            "Try updating Soggfy, or downgrading Spotify to the supported version.",
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