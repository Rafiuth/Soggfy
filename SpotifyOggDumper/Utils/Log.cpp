#include "../pch.h"
#include "Log.h"

#ifdef _DEBUG
LogLevel LogMinLevel = LOG_TRACE;
#else
LogLevel LogMinLevel = LOG_INFO;
#endif

void InstallConsole()
{
    AllocConsole();

    FILE* newFile;
    freopen_s(&newFile, "CONOUT$", "w", stdout);
    freopen_s(&newFile, "CONIN$", "r", stdin);

    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD currMode;
    GetConsoleMode(handle, &currMode);
    SetConsoleMode(handle, currMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    //https://stackoverflow.com/a/45622802
    SetConsoleOutputCP(CP_UTF8);
    //Enable buffering to prevent VS from chopping up UTF-8 byte sequences
    setvbuf(stdout, nullptr, _IOFBF, 1024);
}