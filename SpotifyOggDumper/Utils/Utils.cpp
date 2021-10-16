#include "Utils.h"
#include <chrono>

namespace Utils
{
    void StrReplace(std::string& str, const std::string& needle, const std::string& replacement)
    {
        //https://stackoverflow.com/a/4643526
        size_t index = 0;
        while (true) {
            index = str.find(needle, index);
            if (index == std::string::npos) break;

            str.replace(index, needle.length(), replacement);
            index += replacement.length();
        }
    }

    std::string ExpandEnvVars(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;
        std::wstring srcW = utfConv.from_bytes(str);

        DWORD len = ExpandEnvironmentStrings(srcW.c_str(), NULL, 0);

        std::wstring dstW(len - 1, '\0');
        ExpandEnvironmentStrings(srcW.c_str(), dstW.data(), len);

        return utfConv.to_bytes(dstW);
    }

    std::string RemoveInvalidPathChars(const std::string& src)
    {
        std::string dst;
        dst.reserve(src.length());

        for (char ch : src) {
            if ((ch >= 0x00 && ch < 0x20) || strchr("\\/:*?\"<>|", ch)) {
                continue;
            }
            dst += ch;
        }
        return dst;
    }

    uint32_t StartProcess(const std::string& filename, const std::string& args, const std::string& workDir, bool waitForExit)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;
        std::wstring filenameW = utfConv.from_bytes(filename);
        std::wstring argsW = utfConv.from_bytes("\"" + filename + "\" " + args);
        std::wstring workDirW = utfConv.from_bytes(workDir);

        DWORD exitCode = 0;

        PROCESS_INFORMATION processInfo = {};
        STARTUPINFO startInfo = {};
        startInfo.cb = sizeof(STARTUPINFO);
        startInfo.dwFlags = STARTF_USESHOWWINDOW;
        startInfo.wShowWindow = SW_HIDE;

        DWORD flags = CREATE_NEW_CONSOLE;

        if (!CreateProcess(filenameW.c_str(), argsW.data(), NULL, NULL, FALSE, flags, NULL, workDirW.c_str(), &startInfo, &processInfo)) {
            throw std::runtime_error("Failed to create process " + filename + " (error " + std::to_string(GetLastError()) + ")");
        }
        if (waitForExit) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
        }
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return exitCode;
    }

    int64_t CurrentMillis()
    {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
    }
}