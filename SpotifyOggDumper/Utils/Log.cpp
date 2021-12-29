#include "../pch.h"
#include "Log.h"
#include <fstream>

#if _DEBUG
LogLevel LogMinLevel = LOG_TRACE;
#else
LogLevel LogMinLevel = LOG_DEBUG;
#endif

bool _printToConsole;
std::ofstream _logFile;

void InitLogger(bool printToConsole, const std::filesystem::path& logFile)
{
    _printToConsole = printToConsole;
    _logFile = std::ofstream(logFile, std::ios::trunc);
    
    if (printToConsole) {
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
}
void CloseLogger()
{
    if (_printToConsole) {
        FreeConsole();
    }
    if (_logFile.is_open()) {
        _logFile.close();
    }
}

void _WriteLog(LogLevel level, const std::string& ln)
{
    static const char* prefixes[][2] = {
        { "\033[1;90m", "[TRC] " },
        { "\033[1;37m", "[DBG] " },
        { "\033[1;97m", "[INF] " },
        { "\033[1;93m", "[WRN] " },
        { "\033[1;91m", "[ERR] " }
    };
    if (_printToConsole) {
        std::cout << prefixes[(int)level][0] << ln << "\033[0m" << std::endl;
    }
    if (_logFile.is_open()) {
        _logFile << prefixes[(int)level][1] << ln << std::endl;
    }
}