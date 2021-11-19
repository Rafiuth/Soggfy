#pragma once
#include <Windows.h>
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

        VS_FIXEDFILEINFO* info;
        UINT infoLen;
        VerQueryValue(versionData.get(), L"\\", (LPVOID*)&info, &infoLen);

        return {
            HIWORD(info->dwFileVersionMS),
            LOWORD(info->dwFileVersionMS),
            HIWORD(info->dwFileVersionLS),
            LOWORD(info->dwFileVersionLS),
        };
    }

    std::tuple<int, int, int, int> AsTuple() const
    {
        return std::make_tuple(Major, Minor, Patch, Build);
    }

    std::string AsString() const
    {
        return std::format("{}.{}.{}.{}", Major, Minor, Patch, Build);
    }
};

inline bool operator ==(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() == rhs.AsTuple(); }
inline bool operator >=(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() >= rhs.AsTuple(); }
inline bool operator <=(const AppVersion& lhs, const AppVersion& rhs) { return lhs.AsTuple() <= rhs.AsTuple(); }