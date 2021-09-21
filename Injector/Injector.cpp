#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <shlobj_core.h>
#include <iostream>
#include <functional>
#include <unordered_set>

std::wstring NormalizePath(const std::wstring& path)
{
    DWORD finalLen = GetFullPathName(path.c_str(), 0, NULL, NULL);
    std::wstring fullPath;
    fullPath.resize(finalLen - 1);

    GetFullPathName(path.c_str(), finalLen, (LPWSTR)fullPath.c_str(), NULL);
    return fullPath;
}

BOOL FileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}
LPVOID GetRemoteProcAddress(HANDLE hProc, LPCWSTR moduleName, LPCSTR procName)
{
    //This is fine for a small hack, as kernel32 is always on the same address for all processes.
    //A more arch independent way to do this is to scan the target process modules for the specified
    //moduleName, find the entry point of procName in the PE exports, then return mod.lpBaseOfDll + export.EntryPoint.
    //https://stackoverflow.com/a/22750425
    HMODULE module = LoadLibrary(moduleName);
    return GetProcAddress(module, procName);
}
void InjectDll(HANDLE hProc, const std::wstring& dllPath)
{
    std::wstring fullPath = NormalizePath(dllPath);
    LPCWSTR path = fullPath.c_str();
    DWORD pathLenBytes = (fullPath.size() + 1) * 2;
    
    if (!FileExists(path)) {
        throw std::exception("DLL file does not exist");
    }
    HANDLE loadLibAddr = GetRemoteProcAddress(hProc, L"Kernel32.dll", "LoadLibraryW");
    LPVOID pathArgAddr = VirtualAllocEx(hProc, NULL, pathLenBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pathArgAddr) {
        throw std::exception("Could not allocate memory on target process");
    }
    WriteProcessMemory(hProc, pathArgAddr, path, pathLenBytes, NULL);

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
bool FindSpotifyExePath(std::wstring& exePath)
{
    //Check current directory
    exePath = NormalizePath(L"Spotify.exe");
    if (FileExists(exePath.c_str())) {
        return true;
    }
    //Check %appdata%/Spotify/Spotify.exe
    PWSTR appdataPath;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appdataPath);
    exePath = std::wstring(appdataPath) + L"\\Spotify\\Spotify.exe";
    CoTaskMemFree(appdataPath);

    if (FileExists(exePath.c_str())) {
        return true;
    }
    return false;
}

PROCESS_INFORMATION FindTargetProcess()
{
    PROCESS_INFORMATION target = {};
    target.hProcess = FindMainProcess();

    if (!target.hProcess) {
        std::cout << "Spotify process not found, creating a new one...\n";

        std::wstring exePath;
        std::wstring workDir;
        if (!FindSpotifyExePath(exePath)) {
            throw std::exception("Could not find Spotify exe path");
        }
        workDir = exePath.substr(0, exePath.find_last_of('\\'));

        STARTUPINFO startInfo = {};
        startInfo.cb = sizeof(STARTUPINFO);

        if (!CreateProcess(exePath.c_str(), NULL, NULL, NULL, false, 0, NULL, workDir.c_str(), &startInfo, &target)) {
            throw std::exception("Could not start Spotify process");
        }
    }
    return target;
}
void CleanupTargetProcess(PROCESS_INFORMATION& target)
{
    if (target.hThread) {
        CloseHandle(target.hThread);
    }
    if (target.hProcess) {
        CloseHandle(target.hProcess);
    }
}

int main()
{
    PROCESS_INFORMATION targetProc = {};

    try {
        targetProc = FindTargetProcess();

        std::cout << "Injecting dumper dll into process " << GetProcessId(targetProc.hProcess) << "...\n";

        InjectDll(targetProc.hProcess, L"SpotifyOggDumper.dll");
        std::cout << "Injection succeeded!\n";
    } catch (std::exception& ex) {
        std::cout << "Error: " << ex.what() << "\n";
    }
    CleanupTargetProcess(targetProc);
    return 0;
}