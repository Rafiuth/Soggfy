#include "Utils.h"
#include <chrono>
#include <shellapi.h>
#include <Shlobj.h>

namespace Utils
{
    void Replace(std::string& str, const std::string& needle, const std::string& replacement)
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
    std::vector<std::string_view> Split(std::string_view str, std::string_view delim, bool includeEmptyTokens)
    {
        std::vector<std::string_view> tokens;

        size_t start = 0, end;
        while ((end = str.find(delim, start)) != std::string::npos) {
            if (includeEmptyTokens || start != end) {
                tokens.push_back(str.substr(start, end - start));
            }
            start = end + delim.length();
        }
        if (includeEmptyTokens || start != str.length()) {
            tokens.push_back(str.substr(start));
        }
        return tokens;
    }
    std::string RegexReplace(
        const std::string& input,
        const std::regex& regex,
        std::function<std::string(const std::smatch& match)> format)
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

    const char* FindSubstr(const char* str, int strLen, const char* needle, int needleLen)
    {
        auto strEnd = str + strLen - needleLen;
        for (; str < strEnd; str++) {
            if (memcmp(str, needle, needleLen) == 0) {
                return str;
            }
        }
        return nullptr;
    }

    std::string WideToUtf(std::wstring_view str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(str.data(), str.data() + str.size());
    }
    std::wstring UtfToWide(std::string_view str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.from_bytes(str.data(), str.data() + str.size());
    }
    std::string PathToUtf(const fs::path& path)
    {
        auto utf = path.u8string();
        return std::string((char*)utf.data(), utf.size());
    }

    std::string EncodeBase64(std::string_view data)
    {
        //https://stackoverflow.com/a/6782480
        const char indexToAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const int modTable[] = { 0, 2, 1 };

        size_t len = data.size();
        size_t outLen = 4 * ((len + 2) / 3);
        std::string res(outLen, '\0');

        for (size_t i = 0, j = 0; i < len;) {
            uint8_t octet_a = i < len ? (uint8_t)data[i++] : 0;
            uint8_t octet_b = i < len ? (uint8_t)data[i++] : 0;
            uint8_t octet_c = i < len ? (uint8_t)data[i++] : 0;

            uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

            res[j++] = indexToAlpha[(triple >> 18) & 63];
            res[j++] = indexToAlpha[(triple >> 12) & 63];
            res[j++] = indexToAlpha[(triple >> 6) & 63];
            res[j++] = indexToAlpha[(triple >> 0) & 63];
        }
        for (int i = 0; i < modTable[len % 3]; i++) {
            res[outLen - 1 - i] = '=';
        }
        return res;
    }
    std::string DecodeBase64(std::string_view str)
    {
        //https://stackoverflow.com/a/6782480
        const char alphaToIndex[256] = {
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 0-15
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  //16-31
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 0,  0,  0,  63, //32-47
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  //48-63
            0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, //64-79
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  0,  //80-95
            0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //96-111
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0,  0,  0,  0,  0,  //112-127
        };
        size_t inLen = str.size();
        if (inLen == 0 || inLen % 4 != 0) {
            return "";
        }
        size_t outLen = inLen / 4 * 3;
        size_t pad = 0;
        if (str[inLen - 1] == '=') pad++;
        if (str[inLen - 2] == '=') pad++;

        std::string data(outLen, '\0');

        for (size_t i = 0, j = 0; i < inLen;) {
            uint32_t triple = 
                alphaToIndex[str[i++]] << 18 |
                alphaToIndex[str[i++]] << 12 |
                alphaToIndex[str[i++]] << 6 |
                alphaToIndex[str[i++]] << 0;

            data[j++] = (int8_t)(triple >> 16);
            data[j++] = (int8_t)(triple >> 8);
            data[j++] = (int8_t)(triple >> 0);
        }
        data.resize(outLen - pad);

        return data;
    }

    std::string ExpandEnvVars(const std::string& str)
    {
        std::wstring srcW = UtfToWide(str);

        DWORD len = ExpandEnvironmentStrings(srcW.c_str(), NULL, 0);

        std::wstring dstW(len - 1, '\0');
        ExpandEnvironmentStrings(srcW.c_str(), dstW.data(), len);

        return WideToUtf(dstW);
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

    std::string GetHomeDirectory()
    {
        PWSTR pathW;
        SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &pathW);
        auto path = WideToUtf(pathW);
        CoTaskMemFree(pathW);
        return path;
    }
    void RevealInFileExplorer(const fs::path& path)
    {
        //https://stackoverflow.com/a/54524363
        fs::path normPath = fs::absolute(path);
        PIDLIST_ABSOLUTE pidl;
        SHParseDisplayName(normPath.c_str(), NULL, &pidl, 0, NULL);
        SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
        CoTaskMemFree(pidl);
    }
    void OpenFolderPicker(const fs::path& initialPath, std::function<void(const fs::path&, bool)> callback)
    {
        IFileDialog* fd = NULL;
        bool callbackCalled = false;

        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fd)))) {
            callback(initialPath, true);
            return;
        }
        fd->SetOptions(FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS);
        if (!initialPath.empty()) {
            fs::path normPath = fs::absolute(initialPath);
            IShellItem* item;
            if (SUCCEEDED(SHCreateItemFromParsingName(normPath.c_str(), NULL, IID_PPV_ARGS(&item)))) {
                fd->SetDefaultFolder(item);
                item->Release();
            }
        }
        fd->Show(NULL);

        IShellItem* result;
        if (SUCCEEDED(fd->GetResult(&result))) {
            PWSTR resultPath = NULL;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &resultPath))) {
                callback(fs::path(resultPath), false);
                callbackCalled = true;
                CoTaskMemFree(resultPath);
            }
            result->Release();
        }
        if (!callbackCalled) {
            callback(initialPath, true);
        }
        fd->Release();
    }

    int64_t CurrentMillis()
    {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
    }
}

//https://docs.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
static void ArgvQuote(std::wstring& cmdLine, const std::wstring& arg, bool force = false)
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
static std::wstring CreateCommandLine(const std::vector<std::wstring>& args, const std::wstring& filename)
{
    std::wstring cmdLine;

    if (!filename.empty()) {
        ArgvQuote(cmdLine, filename);
    }
    for (auto& arg : args) {
        if (!cmdLine.empty()) {
            cmdLine.push_back(L' ');
        }
        ArgvQuote(cmdLine, arg);
    }
    return cmdLine;
}
void ProcessBuilder::AddArgs(const std::string& str)
{
    std::wstring strW = Utils::UtfToWide(str);

    int numArgs;
    LPWSTR* args = CommandLineToArgvW(strW.data(), &numArgs);

    for (int i = 0; i < numArgs; i++) {
        _args.push_back(std::wstring(args[i]));
    }
    LocalFree(args);
}
int ProcessBuilder::Start(bool waitForExit)
{
    std::wstring filenameW = _exePath.wstring();
    std::wstring cmdLineW = CreateCommandLine(_args, filenameW);
    std::wstring workDirW = _workDir.wstring();

    DWORD exitCode = 0;

    PROCESS_INFORMATION processInfo = {};
    STARTUPINFO startInfo = {};
    startInfo.cb = sizeof(STARTUPINFO);
    startInfo.dwFlags = STARTF_USESHOWWINDOW;
    startInfo.wShowWindow = SW_HIDE;

    DWORD flags = CREATE_NEW_CONSOLE;

    if (!CreateProcess(filenameW.c_str(), cmdLineW.data(), NULL, NULL, FALSE, flags, NULL, workDirW.c_str(), &startInfo, &processInfo)) {
        throw std::runtime_error("Failed to create process " + _exePath.filename().string() + " (error " + std::to_string(GetLastError()) + ")");
    }
    if (waitForExit) {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    return exitCode;
}

std::string ProcessBuilder::ToString()
{
    return Utils::WideToUtf(CreateCommandLine(_args, _exePath.wstring()));
}