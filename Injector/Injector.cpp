#include <iostream>
#include <filesystem>
#include <functional>
#include <unordered_set>

#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <shlobj_core.h>

namespace fs = std::filesystem;

void InjectDll(HANDLE hProc, const fs::path& dllPath)
{
    fs::path path = fs::absolute(dllPath);
    std::wstring pathW = path.wstring();
    DWORD pathLenBytes = (pathW.size() + 1) * 2;
    
    HANDLE loadLibAddr = GetProcAddress(GetModuleHandle(L"Kernel32.dll"), "LoadLibraryW");
    LPVOID pathArgAddr = VirtualAllocEx(hProc, NULL, pathLenBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pathArgAddr) {
        throw std::exception("Could not allocate memory on target process");
    }
    WriteProcessMemory(hProc, pathArgAddr, pathW.data(), pathLenBytes, NULL);

    HANDLE thread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, pathArgAddr, 0, NULL);
    if (!thread) {
        throw std::exception("Could not create remote thread");
    }
    if (WaitForSingleObject(thread, 10000) == WAIT_FAILED) {
        throw std::exception("Thread exit timeout");
    }
    DWORD exitCode;
    GetExitCodeThread(thread, &exitCode);
    if (exitCode == 0) {
        throw std::exception("Failed to load library from remote process");
    }
    //FIXME: we should probably free these handles before throwing exceptions
    VirtualFreeEx(hProc, pathArgAddr, 0, MEM_RELEASE);
    CloseHandle(thread);
}

void EnumProcessesEx(std::function<void(PROCESSENTRY32&)> visitor)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 proc = {};
    proc.dwSize = sizeof(proc);

    if (Process32First(snapshot, &proc)) {
        do {
            visitor(proc);
        } while (Process32Next(snapshot, &proc));
    }
    CloseHandle(snapshot);
}
bool HasOpenWindow(DWORD procId)
{
    struct Data { DWORD ProcId; HWND Win; };
    Data data = { procId, 0 };

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto data = (Data*)param;
        DWORD parentProcId;
        GetWindowThreadProcessId(hwnd, &parentProcId);

        if (parentProcId == data->ProcId && GetWindow(hwnd, GW_OWNER) == 0 && IsWindowVisible(hwnd)) {
            data->Win = hwnd;
            return false;
        }
        return true;
    }, (LPARAM)&data);

    return data.Win != 0;
}
//Note: this will only work if both processes have the same word size (64 vs 32)
std::wstring GetProcessCommandLine(HANDLE hProc)
{
    typedef NTSTATUS(NTAPI* NtQueryInformationProcess_FuncType)(
        IN HANDLE ProcessHandle,
        ULONG ProcessInformationClass,
        OUT PVOID ProcessInformation,
        IN ULONG ProcessInformationLength,
        OUT PULONG ReturnLength OPTIONAL
    );
    static const auto _QueryProcInfo = (NtQueryInformationProcess_FuncType)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationProcess");

    PROCESS_BASIC_INFORMATION info;
    if (!NT_SUCCESS(_QueryProcInfo(hProc, ProcessBasicInformation, &info, sizeof(info), NULL))) {
        throw std::exception("Failed to get process information");
    }
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS procParams;
    ReadProcessMemory(hProc, info.PebBaseAddress, &peb, sizeof(peb), NULL);
    ReadProcessMemory(hProc, peb.ProcessParameters, &procParams, sizeof(procParams), NULL);

    std::wstring cmdLine(procParams.CommandLine.Length, '\0');
    ReadProcessMemory(hProc, procParams.CommandLine.Buffer, cmdLine.data(), cmdLine.size(), NULL);
    return cmdLine;
}

#define COL_RED     "\033[1;91m"
#define COL_GREEN   "\033[1;92m"
#define COL_YELLOW  "\033[1;93m"
#define COL_BLUE    "\033[1;94m"
#define COL_RESET   "\033[0m"

HANDLE FindSpotifyProcess()
{
    DWORD procId = 0;
    
    EnumProcessesEx([&](auto proc) {
        if (_wcsicmp(proc.szExeFile, L"Spotify.exe") == 0 && HasOpenWindow(proc.th32ProcessID)) {
            procId = proc.th32ProcessID;
        }
    });
    if (procId == 0) {
        throw std::exception("Couldn't find target process");
    }
    return OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION |  PROCESS_VM_READ |  PROCESS_VM_WRITE, 
        false, procId
    );
}
HANDLE LaunchSpotifyProcess(bool enableRemoteDebug)
{
    fs::path exePath = fs::absolute("Spotify/Spotify.exe");
    fs::path workDir = exePath.parent_path();
    if (!fs::exists(exePath)) {
        throw std::exception("Spotify installation not found. Run Install.ps1 and try again.");
    }
    std::cout << "Launching Spotify...\n";

    PROCESS_INFORMATION proc = {};
    STARTUPINFO startInfo = {};
    startInfo.cb = sizeof(STARTUPINFO);

    std::wstring cmdLine = L"\"" + exePath.wstring() + L"\" ";
    if (enableRemoteDebug) {
        cmdLine += L" --remote-debugging-port=9222";
    }

    if (!CreateProcess(exePath.c_str(), cmdLine.data(), NULL, NULL, false, 0, NULL, workDir.c_str(), &startInfo, &proc)) {
        throw std::exception("Could not start Spotify process");
    }
    //Wait a bit until the window is open
    for (int i = 0; !HasOpenWindow(proc.dwProcessId); i++) {
        if (i >= 30) { //15s
            std::cout << COL_YELLOW << "Spotify is taking too long to open. Try re-injecting if Soggfy doesn't load properly.\n" COL_RESET;
            break;
        }
        Sleep(500);
    }
    CloseHandle(proc.hThread);
    return proc.hProcess;
}

void KillSpotifyProcesses()
{
    EnumProcessesEx([&](auto proc) {
        if (_wcsicmp(proc.szExeFile, L"Spotify.exe") != 0) return;

        auto handle = OpenProcess(PROCESS_TERMINATE, false, proc.th32ProcessID);
        TerminateProcess(handle, 0);
        WaitForSingleObject(handle, 3000);
        CloseHandle(handle);
    });
}
//Delete `%localappdata%/Spotify/Update` to prevent Spotify from auto updating
void DeleteSpotifyUpdate()
{
    PWSTR localAppData;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData);

    auto updateDir = fs::path(localAppData) / "Spotify/Update";
    std::error_code errCode;
    //remove_all() returns 0 if the path doesn't exist, -1 on error.
    if (fs::remove_all(updateDir, errCode) == static_cast<uintmax_t>(-1)) {
        std::cout << COL_YELLOW "Warn: Failed to delete Spotify update (%localappdata%/Spotify/Update). You may need to re-run Install.ps1 to downgrade.\n" COL_RESET;
    }
    CoTaskMemFree(localAppData);
}

void EnableAnsiColoring()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD currMode;
    GetConsoleMode(handle, &currMode);
    SetConsoleMode(handle, currMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

int main(int argc, char* argv[])
{
    bool launch = false;
    bool enableRemoteDebug = false;
    for (int i = 0; i < argc; i++) {
        launch |= strcmp(argv[i], "-l") == 0;
        enableRemoteDebug |= strcmp(argv[i], "-d") == 0;
    }
    
    EnableAnsiColoring();

    try {
        HANDLE targetProc;
        if (!launch) {
            targetProc = FindSpotifyProcess();
        } else {
            KillSpotifyProcesses();
            DeleteSpotifyUpdate();
            targetProc = LaunchSpotifyProcess(enableRemoteDebug);
        }
        std::cout << "Injecting dumper dll into Spotify process (" << GetProcessId(targetProc) << ")...\n";

        InjectDll(targetProc, L"SpotifyOggDumper.dll");
        CloseHandle(targetProc);
        
        std::cout << COL_GREEN "Injection succeeded!\n" COL_RESET;
    } catch (std::exception& ex) {
        std::cout << COL_RED "Error: " << ex.what() << "\n" COL_RESET;
    }
    
    for (int i = 5; i > 0; i--) {
        std::cout << "Exiting in " << i << "s...\r";
        Sleep(1000);
    }
    return 0;
}