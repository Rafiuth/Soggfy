#pragma once
#include "../pch.h"

namespace Utils
{
    void StrReplace(std::string& str, const std::string& needle, const std::string& replacement);

    //Call WINAPI ExpandEnvironmentStrings(), assuming str is UTF8.
    std::string ExpandEnvVars(const std::string& str);

    std::string RemoveInvalidPathChars(const std::string& src);

    uint32_t StartProcess(const std::string& filename, const std::string& args, const std::string& workDir, bool waitForExit = false);

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