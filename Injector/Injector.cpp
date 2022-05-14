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
void EnumWindowsEx(std::function<BOOL(HWND)> visitor)
{
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        return (*(std::function<BOOL(HWND)>*)param)(hwnd);
    }, (LPARAM)&visitor);
}

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

void KillCrashpadProcess()
{
    EnumProcessesEx([&](auto proc) {
        if (_wcsicmp(proc.szExeFile, L"Spotify.exe") == 0) {
            DWORD accFlags =
                PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION |
                PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE;
            HANDLE hProc = OpenProcess(accFlags, false, proc.th32ProcessID);
            std::wstring cmdLine = GetProcessCommandLine(hProc);
            if (cmdLine.find(L"--type=crashpad-handler") != std::wstring::npos) {
                TerminateProcess(hProc, 0);
                std::cout << "Spotify crash reporter process killed successfully (" << proc.th32ProcessID << ")\n";
            }
            CloseHandle(hProc);
        }
    });
}

HANDLE FindMainProcess()
{
    //Find spotify processes
    std::unordered_set<DWORD> possibleProcs;
    EnumProcessesEx([&](auto proc) {
        if (_wcsicmp(proc.szExeFile, L"Spotify.exe") == 0) {
            possibleProcs.emplace(proc.th32ProcessID);
        }
    });
    //Find the process with a open window
    DWORD actualProcId = 0;
    EnumWindowsEx([&](HWND hwnd) {
        DWORD procId;
        GetWindowThreadProcessId(hwnd, &procId);

        if (possibleProcs.find(procId) != possibleProcs.end()) {
            actualProcId = procId;
            return false;
        }
        return true;
    });

    if (!actualProcId) {
        return NULL;
    }
    return OpenProcess(
        PROCESS_QUERY_INFORMATION | 
        PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION | 
        PROCESS_VM_READ | 
        PROCESS_VM_WRITE, 
        false, actualProcId
    );
}
bool FindSpotifyExePath(fs::path& exePath)
{
    //Check current directory
    if (fs::exists(exePath.c_str())) {
        return true;
    }
    //Check %appdata%/Spotify/Spotify.exe
    PWSTR appdataPath;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appdataPath);
    exePath = std::wstring(appdataPath) + L"\\Spotify\\Spotify.exe";
    CoTaskMemFree(appdataPath);

    if (fs::exists(exePath)) {
        return true;
    }
    return false;
}

PROCESS_INFORMATION FindTargetProcess()
{
    PROCESS_INFORMATION target = {};
    target.hProcess = FindMainProcess();

    if (!target.hProcess) {
        std::cout << "Launching Spotify...\n";

        fs::path exePath;
        if (!FindSpotifyExePath(exePath)) {
            throw std::exception("Could not find Spotify exe path");
        }
        fs::path workDir = exePath.parent_path();

        STARTUPINFO startInfo = {};
        startInfo.cb = sizeof(STARTUPINFO);

        if (!CreateProcess(exePath.c_str(), NULL, NULL, NULL, false, 0, NULL, workDir.c_str(), &startInfo, &target)) {
            throw std::exception("Could not start Spotify process");
        }
        //Wait a bit until the window is open
        for (int i = 0; i < 30; i++) {
            HANDLE handle = FindMainProcess();
            if (handle) {
                CloseHandle(handle);
                break;
            }
            Sleep(500);
        }
    }
    return target;
}
void CloseProcess(PROCESS_INFORMATION& target)
{
    if (target.hThread) {
        CloseHandle(target.hThread);
    }
    if (target.hProcess) {
        CloseHandle(target.hProcess);
    }
}

#define COL_RED     "\033[1;91m"
#define COL_GREEN   "\033[1;92m"
#define COL_YELLOW  "\033[1;93m"
#define COL_BLUE    "\033[1;94m"
#define COL_RESET   "\033[0m"
void EnableAnsiParsing()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD currMode;
    GetConsoleMode(handle, &currMode);
    SetConsoleMode(handle, currMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
bool IsFFmpegInstalled()
{
    //Try Soggfy/ffmpeg/ffmpeg.exe
    if (fs::exists("ffmpeg/ffmpeg.exe")) {
        return true;
    }
    //Try %PATH%
    std::wstring envPath;
    DWORD envPathLen = SearchPath(NULL, L"ffmpeg.exe", NULL, 0, envPath.data(), NULL);
    if (envPathLen != 0) {
        return true;
    }
    return false;
}

int main()
{
    EnableAnsiParsing();
    PROCESS_INFORMATION targetProc = {};

    try {
        targetProc = FindTargetProcess();

        std::cout << "Injecting dumper dll into process " << GetProcessId(targetProc.hProcess) << "...\n";

        InjectDll(targetProc.hProcess, L"SpotifyOggDumper.dll");
        KillCrashpadProcess();
        std::cout << COL_GREEN "Injection succeeded!\n" COL_RESET;

        if (!IsFFmpegInstalled()) {
            std::cout << COL_YELLOW "FFmpeg not found, songs won't be tagged nor converted.\n";
            std::cout << COL_YELLOW "Run DownloadFFmpeg.ps1 or add ffmpeg to the PATH environment variable.\n" COL_RESET;
        }
    } catch (std::exception& ex) {
        std::cout << COL_RED "Error: " << ex.what() << "\n" COL_RESET;
    }
    CloseProcess(targetProc);
    
    for (int i = 3; i > 0; i--) {
        std::cout << "Exiting in " << i << "s...\r";
        Sleep(1000);
    }
    return 0;
}