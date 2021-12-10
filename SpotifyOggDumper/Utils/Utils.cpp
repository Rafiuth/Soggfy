#include "Utils.h"
#include <chrono>
#include <shellapi.h>

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
    std::string StrRegexReplace(
        const std::string& input,
        const std::regex& regex,
        std::function<std::string(std::smatch const& match)> format)
    {
        //https://stackoverflow.com/a/57420759
        std::ostringstream output;
        std::sregex_iterator begin(input.begin(), input.end(), regex), end;
        ptrdiff_t endPos = 0;

        for (; begin != end; begin++) {
            output << begin->prefix() << format(*begin);
            endPos = begin->position() + begin->length();
        }
        output << input.substr(endPos);
        return output.str();
    }

    const char* FindPosition(const char* str, int strLen, const char* needle, int needleLen)
    {
        auto strEnd = str + strLen - needleLen;
        for (; str < strEnd; str++) {
            if (memcmp(str, needle, needleLen) == 0) {
                return str;
            }
        }
        return nullptr;
    }

    std::string StrWideToUtf(std::wstring_view str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(str.data(), str.data() + str.size());
    }
    std::wstring StrUtfToWide(std::string_view str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.from_bytes(str.data(), str.data() + str.size());
    }

    std::string PathToUtf(const fs::path& path)
    {
        auto utf = path.u8string();
        return std::string((char*)utf.data(), utf.size());
    }

    std::string ExpandEnvVars(const std::string& str)
    {
        std::wstring srcW = StrUtfToWide(str);

        DWORD len = ExpandEnvironmentStrings(srcW.c_str(), NULL, 0);

        std::wstring dstW(len - 1, '\0');
        ExpandEnvironmentStrings(srcW.c_str(), dstW.data(), len);

        return StrWideToUtf(dstW);
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

    uint32_t StartProcess(const fs::path& filename, const std::vector<std::wstring>& args, const fs::path& workDir, bool waitForExit)
    {
        std::wstring filenameW = filename.wstring();
        std::wstring argsW = CreateCommandLine(args, filename);
        std::wstring workDirW = workDir.wstring();

        DWORD exitCode = 0;

        PROCESS_INFORMATION processInfo = {};
        STARTUPINFO startInfo = {};
        startInfo.cb = sizeof(STARTUPINFO);
        startInfo.dwFlags = STARTF_USESHOWWINDOW;
        startInfo.wShowWindow = SW_HIDE;

        DWORD flags = CREATE_NEW_CONSOLE;

        if (!CreateProcess(filenameW.c_str(), argsW.data(), NULL, NULL, FALSE, flags, NULL, workDirW.c_str(), &startInfo, &processInfo)) {
            throw std::runtime_error("Failed to create process " + filename.string() + " (error " + std::to_string(GetLastError()) + ")");
        }
        if (waitForExit) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
        }
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return exitCode;
    }

    //https://docs.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
    void ArgvQuote(std::wstring& cmdLine, const std::wstring& arg, bool force = false)
    {
        // Unless we're told otherwise, don't quote unless we actually
        // need to do so --- hopefully avoid problems if programs won't
        // parse quotes properly
        if (!force && !arg.empty() && arg.find_first_of(L" \t\n\v\"") == arg.npos) {
            cmdLine.append(arg);
        } else {
            cmdLine.push_back(L'"');

            for (auto it = arg.begin(); ; ++it) {
                int numBackslashes = 0;

                while (it != arg.end() && *it == L'\\') {
                    ++it;
                    ++numBackslashes;
                }

                if (it == arg.end()) {
                    // Escape all backslashes, but let the terminating
                    // double quotation mark we add below be interpreted
                    // as a metacharacter.
                    cmdLine.append(numBackslashes * 2, L'\\');
                    break;
                } else if (*it == L'"') {
                    // Escape all backslashes and the following
                    // double quotation mark.
                    cmdLine.append(numBackslashes * 2 + 1, L'\\');
                    cmdLine.push_back(*it);
                } else {
                    // Backslashes aren't special here.
                    cmdLine.append(numBackslashes, L'\\');
                    cmdLine.push_back(*it);
                }
            }
            cmdLine.push_back(L'"');
        }
    }
    void SplitCommandLine(const std::wstring& cmdLine, std::vector<std::wstring>& dest)
    {
        int numArgs;
        LPWSTR* args = CommandLineToArgvW(cmdLine.data(), &numArgs);

        for (int i = 0; i < numArgs; i++) {
            dest.push_back(std::wstring(args[i]));
        }
        LocalFree(args);
    }
    std::wstring CreateCommandLine(std::vector<std::wstring> args, fs::path filename)
    {
        std::wstring cmdLine;

        if (!filename.empty()) {
            ArgvQuote(cmdLine, filename.wstring());
        }
        for (auto& arg : args) {
            if (!cmdLine.empty()) {
                cmdLine.push_back(L' ');
            }
            ArgvQuote(cmdLine, arg);
        }
        return cmdLine;
    }

    int64_t CurrentMillis()
    {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
    }
}