#pragma once
#include "../pch.h"
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

enum class FilePickerType
{
    OPEN_FILE       = 1,
    SAVE_FILE       = 2,
    SELECT_FOLDER   = 3
};

namespace Utils
{
    void Replace(std::string& str, const std::string& needle, const std::string& replacement);
    std::vector<std::string_view> Split(std::string_view str, std::string_view delim, bool includeEmptyTokens = true);

    std::string RegexReplace(
        const std::string& input,
        const std::regex& regex,
        std::function<std::string(const std::smatch& match)> format
    );

    const char* FindSubstr(const char* str, int strLen, const char* needle, int needleLen);

    std::string WideToUtf(std::wstring_view str);
    std::wstring UtfToWide(std::string_view str);
    std::string PathToUtf(const fs::path& path);

    std::string EncodeBase64(std::string_view data);
    std::string DecodeBase64(std::string_view str);

    //Call WINAPI ExpandEnvironmentStrings(), assuming str is UTF8.
    std::string ExpandEnvVars(const std::string& str);

    std::string GetMusicFolder();
    void RevealInFileExplorer(const fs::path& path);

    /**
     * @brief Opens a file browser dialog.
     * @param saveTypes A list of save filters in the format "DisplayName|*.ext"
     */
    fs::path OpenFilePicker(FilePickerType type, const fs::path& initialPath, const std::vector<std::string>& fileTypes = {});

    int64_t CurrentMillis();
}

class ProcessBuilder
{
    fs::path _exePath;
    fs::path _workDir;
    std::vector<std::wstring> _args;

public:
    void SetExePath(const fs::path& path) { _exePath = path; }
    void SetWorkDir(const fs::path& path) { _workDir = path; }

    void AddArgPath(const fs::path& path) { _args.push_back(path.wstring()); }
    void AddArg(const std::string& str) { _args.push_back(Utils::UtfToWide(str)); }

    void AddArgs(const std::string& str);

    int Start(bool waitForExit = false);

    std::string ToString();
};

namespace nlohmann
{
    template<>
    struct adl_serializer<fs::path>
    {
        static void to_json(json& j, const fs::path& val)
        {
            j = Utils::PathToUtf(val);
        }
        static void from_json(const json& j, fs::path& val)
        {
            val = fs::u8path(j.get_ref<const json::string_t&>());
        }
    };
}