#pragma once
#include "../pch.h"
#include <filesystem>

namespace Utils
{
    namespace fs = std::filesystem;

    void StrReplace(std::string& str, const std::string& needle, const std::string& replacement);
    std::string StrRegexReplace(
        const std::string& input,
        const std::regex& regex,
        std::function<std::string(std::smatch const& match)> format);

    const char* FindPosition(const char* str, int strLen, const char* needle, int needleLen);

    std::string StrWideToUtf(std::wstring_view str);
    std::wstring StrUtfToWide(std::string_view str);

    std::string PathToUtf(const fs::path& path);

    //Call WINAPI ExpandEnvironmentStrings(), assuming str is UTF8.
    std::string ExpandEnvVars(const std::string& str);

    std::string RemoveInvalidPathChars(const std::string& src);

    uint32_t StartProcess(const fs::path& filename, const std::vector<std::wstring>& args, const fs::path& workDir, bool waitForExit = false);
    void SplitCommandLine(const std::wstring& cmdLine, std::vector<std::wstring>& dest);
    std::wstring CreateCommandLine(std::vector<std::wstring> args, fs::path filename = {});

    int64_t CurrentMillis();
}

struct TimeRange
{
    int64_t Start = 0, End = 0;

    inline void MarkStart() { Start = Utils::CurrentMillis(); }
    inline void MarkEnd() { End = Utils::CurrentMillis(); }
    inline bool IsMarked() { return Start != 0 && End != 0; }

    inline int64_t GetOverlappingMillis(const TimeRange& other)
    {
        //a:    s...e
        //b: s....e
        //r:    ~~~
        int64_t start = std::max(Start, other.Start);
        int64_t end = std::min(End, other.End);
        return std::max(end - start, 0LL);
    }
};