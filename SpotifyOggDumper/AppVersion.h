#pragma once
#include <Windows.h>
#include <iostream>
#include <tuple>
#include <format>

struct AppVersion
{
    int Major, Minor, Patch, Build;

    static AppVersion Of(const std::wstring& fn)
    {
        int versionDataLen = GetFileVersionInfoSize(fn.c_str(), NULL);
        auto versionData = std::make_unique<uint8_t[]>(versionDataLen);
        GetFileVersionInfo(fn.c_str(), 0, versionDataLen, versionData.get());

        VS_FIXEDFILEINFO* versionNum;
        UINT versionNumLen;
        VerQueryValue(versionData.get(), L"\\", (LPVOID*)&versionNum, &versionNumLen);

        return {
            HIWORD(versionNum->dwFileVersionMS),
            LOWORD(versionNum->dwFileVersionMS),
            HIWORD(versionNum->dwFileVersionLS),
            LOWORD(versionNum->dwFileVersionLS),
        };
    }

    std::tuple<int, int, int, int> AsTuple() const
    {
        return std::make_tuple(Major, Minor, Patch, Build);
    }
};

//This is bad, I know...
inline bool operator ==(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() == rhs.AsTuple(); }
inline bool operator >=(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() >= rhs.AsTuple(); }
inline bool operator <=(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() <= rhs.AsTuple(); }

inline std::ostream& operator <<(std::ostream& st, const AppVersion& ver)
{
    st << ver.Major << "." << ver.Minor << "." << ver.Patch << "." << ver.Build;
    return st;
}
template <>
struct std::formatter<AppVersion> : std::formatter<std::string>
{
    auto format(AppVersion ver, format_context& ctx)
    {
        return formatter<string>::format(std::format("{}.{}.{}.{}", ver.Major, ver.Minor, ver.Patch, ver.Build), ctx);
    }
};