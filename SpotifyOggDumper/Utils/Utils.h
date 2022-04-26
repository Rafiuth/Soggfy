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

    fs::path GetAppDataFolder();
    fs::path GetMusicFolder();
    void RevealInFileExplorer(const fs::path& path);

    /**
     * @brief Opens a file browser dialog.
     * @param saveTypes A list of save filters in the format "DisplayName|*.ext"
     */
    fs::path OpenFilePicker(FilePickerType type, const fs::path& initialPath, const std::vector<std::string>& fileTypes = {});

    /**
     * @brief Prepends "\\?\" to the specified path (for long path support) 
     *        if it is longer than 256 characters or |force == true|.
     */
    fs::path NormalizeToLongPath(const fs::path& path, bool force = false);

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

    //Note: |logStdout| will cause the stdout to be logged on debug level, implies |waitForExit|.
    int Start(bool waitForExit = false, bool logStdout = false);

    std::string ToString();
};

namespace nlohmann
{
    template<>
    struct adl_serializer<fs::path>
    {
        static void to_json(json& j, const fs::path& val)
        {
            auto str = Utils::PathToUtf(val);
            j = str.starts_with(R"(\\?\)") ? str.substr(4) : str;
        }
        static void from_json(const json& j, fs::path& val)
        {
            auto& str = j.get_ref<const json::string_t&>();
            val = Utils::NormalizeToLongPath(fs::u8path(str), false);
        }
    };
}