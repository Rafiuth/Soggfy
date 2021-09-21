#include "../pch.h"
#include "Log.h"

LogLevel LogMinLevel = LOG_INFO;

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
}