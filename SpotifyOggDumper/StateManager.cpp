#include "pch.h"
#include <fstream>
#include <regex>
#include <deque>

#include "StateManager.h"
#include "ControlServer.h"
#include "CefUtils.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"

struct PlaybackInfo
{
    std::string Id;
    fs::path FileName;
    std::ofstream FileStream;
    std::string ActualExt;
    std::pair<std::string, std::string> Status; //(Status, Message)
    bool Discard = false;
    bool ReadyToSave = false;
};
//TODO: move most of playback state handling to TS
struct StateManagerImpl : public StateManager
{
    //TODO: fix potential race conditions with these (mutex is only locked when accessing this map)
    std::unordered_map<std::string, std::shared_ptr<PlaybackInfo>> _playbacks;
    std::mutex _mutex;

    json _config;

    fs::path _dataDir;
    fs::path _configPath;
    fs::path _ffmpegPath;

    ControlServer _ctrlSv;

    StateManagerImpl(const fs::path& dataDir) :
        _dataDir(dataDir),
        _ctrlSv(std::bind(&StateManagerImpl::HandleMessage, this, std::placeholders::_1, std::placeholders::_2))
    {
        _configPath = _dataDir / "config.json";

        std::ifstream configFile(_configPath);
        if (configFile.good()) {
            _config = json::parse(configFile, nullptr, true, true);
        } else {
            _config = json::object();
        }

        fs::path baseSavePath = _config.value("/savePaths/basePath"_json_pointer, fs::path());
        if (!fs::exists(baseSavePath)) {
            baseSavePath = Utils::GetMusicFolder() / "Soggfy";
            fs::create_directories(baseSavePath);
            _config["savePaths"]["basePath"] = baseSavePath;
        }
        //delete old temp files
        std::error_code deleteError;
        fs::remove_all(dataDir / "temp", deleteError);

        _ffmpegPath = FindFFmpegPath();
        if (_ffmpegPath.empty()) {
            LogWarn("FFmpeg binaries were not found, downloaded files won't be tagged nor converted. Run DownloadFFmpeg.ps1 to fix this.");
        }
        LogDebug("FFmpeg path: {}", _ffmpegPath.string());
    }

    void RunControlServer()
    {
        _ctrlSv.Run();
    }

    void HandleMessage(Connection* conn, Message&& msg)
    {
        auto& content = msg.Content;

        LogTrace("ControlMessage: {} {} + {} bytes", (int)msg.Type, msg.Content.dump(), msg.BinaryContent.size());

        switch (msg.Type) {
            case MessageType::SERVER_OPEN: {
                std::ifstream srcJsFile(_dataDir / "Soggfy.js", std::ios::binary);

                if (srcJsFile.good()) {
                    LogInfo("Injecting JS...");
                    std::string srcJs(std::istreambuf_iterator<char>(srcJsFile), {});
                    Utils::Replace(srcJs, "ws://127.0.0.1:28653/sgf_ctrl", msg.Content["addr"]);
                    CefUtils::InjectJS(srcJs);
                }
                LogInfo("Ready");
                break;
            }
            case MessageType::HELLO: {
                conn->Send(MessageType::SYNC_CONFIG, _config);
                break;
            }
            case MessageType::BYE: {
                break;
            }
            case MessageType::SYNC_CONFIG: {
                for (auto& [key, val] : content.items()) {
                    _config[key] = val;
                }
                std::ofstream(_configPath) << _config.dump(4);

                if (!_config.value("downloaderEnabled", true)) {
                    std::lock_guard<std::mutex> lock(_mutex);

                    for (auto& [id, pb] : _playbacks) {
                        DiscardTrack(*pb, "Listen mode was toggled");
                    }
                }
                break;
            }
            case MessageType::TRACK_META: {
                std::string playbackId = content["playbackId"];
                auto playback = GetPlayback(playbackId);
                if (!playback || !playback->ReadyToSave) break;

                //delay closing the file to avoid truncating the track
                playback->FileStream.close();

                if (!content.value("failed", false)) {
                    std::thread(&StateManagerImpl::SaveTrack, this, playback, msg).detach();
                } else {
                    LogInfo("Discarding playback {}: {}", playbackId, content.value("message", "null"));
                }
                RemovePlayback(playbackId);
                break;
            }
            case MessageType::DOWNLOAD_STATUS: {
                if (content.contains("playbackId")) {
                    auto playback = GetPlayback(content["playbackId"]);
                    if (!playback) break;

                    if (content.value("ignore", false)) {
                        DiscardTrack(*playback, "Ignored");
                    }
                    conn->Send(MessageType::DOWNLOAD_STATUS, {
                        { "playbackId", playback->Id },
                        { "status", playback->Status.first },
                        { "message", playback->Status.second }
                    });
                }
                else if (content.contains("searchTree")) {
                    json results = json::object();
                    auto basePath = Utils::NormalizeToLongPath(content["basePath"], true);
                    auto relativeTo = content.contains("relativeTo") 
                        ? Utils::NormalizeToLongPath(content["relativeTo"], true)
                        : fs::path();
                    SearchPathTree(results, content["searchTree"], basePath, relativeTo);

                    conn->Send(MessageType::DOWNLOAD_STATUS, { 
                        { "reqId", content["reqId"] },
                        { "results", results }
                    });
                }
                break;
            }
            case MessageType::OPEN_FOLDER: {
                Utils::RevealInFileExplorer(content["path"]);
                break;
            }
            case MessageType::OPEN_FILE_PICKER: {
                std::thread([this, ct = content]() {
                    fs::path initialPath = ct["initialPath"];
                    auto fileTypes = ct.value("fileTypes", std::vector<std::string>());
                    auto selectedPath = Utils::OpenFilePicker(ct["type"], initialPath, fileTypes);
                    bool success = !selectedPath.empty();

                    //FIXME: broadcasting because conn could be freed before this thread finishes
                    _ctrlSv.Broadcast(MessageType::OPEN_FILE_PICKER, {
                        { "reqId", ct["reqId"] },
                        { "path", success ? selectedPath : initialPath },
                        { "success", success }
                    });
                }).detach();
                break;
            }
            case MessageType::WRITE_FILE: {
                fs::path path = content["path"];
                std::string mode = content.value("mode", "");

                if (!(mode == "keep" && fs::exists(path))) {
                    fs::create_directories(path.parent_path());

                    int flags = std::ios::binary | (mode == "append" ? std::ios::app : std::ios::trunc);
                    std::ofstream ofs(path, flags);
                    if (content.contains("text")) {
                        ofs << content["text"].get_ref<const json::string_t&>();
                    } else {
                        ofs << msg.BinaryContent;
                    }
                }
                auto& reqId = content["reqId"];
                if (!reqId.is_null()) {
                    conn->Send(MessageType::WRITE_FILE, {
                        { "reqId", reqId },
                        { "success", true }
                    });
                }
                break;
            }
            default: {
                throw std::runtime_error("Unexpected message (type=" + std::to_string((int)msg.Type) + ")");
            }
        }
    }

    void ReceiveAudioData(const std::string& playbackId, const char* data, int length)
    {
        auto playback = GetPlayback(playbackId);
        if (!playback || playback->Discard) return;

        auto& fs = playback->FileStream;

        if (!fs.is_open()) {
            LogWarn("Received audio data after playback ended (this should never happen).");
            DiscardTrack(*playback, "Stream truncated");
            return;
        }
        if (fs.tellp() == 0) {
            if (memcmp(data, "OggS", 4) == 0) {
                //skip spotify's custom ogg page (it sets the EOS flag, causing players to think the file is corrupt)
                auto nextPage = Utils::FindSubstr(data + 4, length - 4, "OggS", 4);

                if (nextPage) {
                    length -= nextPage - data;
                    data = nextPage;
                } else {
                    //this might happen if the buffer is too small.
                    LogWarn("Could not skip Spotify's custom OGG page. Downloaded file might be broken. (p#{})", playback->Id);
                }
                playback->ActualExt = "ogg";
            } else if (memcmp(data, "ID3", 3) == 0 || (data[0] == 0xFF && (data[1] & 0xF0) == 0xF0)) {
                playback->ActualExt = "mp3";
            }
        }
        fs.write(data, length);
    }

    void OnTrackCreated(const std::string& playbackId, double& speed)
    {
        if (!_config.value("downloaderEnabled", true)) return;

        CreatePlayback(playbackId);

        double newSpeed = _config.value("playbackSpeed", 0.0);
        if (newSpeed > 0) {
            speed = newSpeed;
        }
    }
    void OnTrackDone(const std::string& playbackId)
    {
        auto playback = GetPlayback(playbackId);
        if (!playback) return;

        playback->ReadyToSave = true;

        if (playback->Discard) {
            std::error_code err;
            fs::remove(playback->FileName, err);

            RemovePlayback(playbackId);
            return;
        }
        if (playback->ActualExt.empty()) {
            LogWarn("Unrecognized audio codec in playback {}. Try changing streaming quality.", playbackId);
            SendTrackStatus(playback->Id, "ERROR", "Unrecognized audio codec");
            return;
        }
        LogDebug("Requesting metadata for playback {}...", playbackId);
        _ctrlSv.Broadcast(MessageType::TRACK_META, { { "playbackId", playbackId } });
    }
    void DiscardTrack(const std::string& playbackId, const std::string& reason)
    {
        auto playback = GetPlayback(playbackId);
        if (!playback) return;

        DiscardTrack(*playback, reason);
    }
    void DiscardTrack(PlaybackInfo& playback, const std::string& reason)
    {
        if (playback.Discard) return;

        auto status = std::make_pair("ERROR", "Canceled: " + reason);
        playback.Discard = true;
        playback.Status = status;
        SendTrackStatus(playback.Id, status.first, status.second);
    }
    bool IsUrlBlocked(std::wstring_view url)
    {
        if (url.starts_with(L"https://upgrade.scdn.co/")) {
            return true;
        }
        if (_config.value("blockAds", true)) {
            //https://github.com/abba23/spotify-adblock/blob/main/config.toml#L73
            return url.starts_with(L"https://spclient.wg.spotify.com/ads/") ||
                   url.starts_with(L"https://spclient.wg.spotify.com/ad-logic/") ||
                   url.starts_with(L"https://spclient.wg.spotify.com/gabo-receiver-service/");
        }
        return false;
    }

    void Shutdown()
    {
        for (auto& [id, playback] : _playbacks) {
            playback->FileStream.close();
        }
        _ctrlSv.Stop();
    }
    
    void SaveTrack(std::shared_ptr<PlaybackInfo> playback, Message msg)
    {
        json& ct = msg.Content;
        json& meta = ct["metadata"];
        std::string trackUri = ct.value("trackUri", "null");
        std::string trackName = meta.value("album_artist", "null") + " - " + meta.value("title", "null");

        try {
            SendTrackStatus(trackUri, "CONVERTING", "Converting...");
            LogInfo("Saving track {}", trackName);
            LogDebug("  stream: {}", Utils::PathToUtf(playback->FileName.filename()));
            
            fs::path trackPath = ct["trackPath"];
            fs::path coverPath = ct["coverPath"];
            fs::path tmpCoverPath = MakeTempPath(ct["coverTempPath"]);

            //always cache cover art
            if (!fs::exists(tmpCoverPath)) {
                std::ofstream(tmpCoverPath, std::ios::binary) << msg.BinaryContent;
            }
            fs::create_directories(trackPath.parent_path());

            if (!coverPath.empty()) {
                fs::copy_file(tmpCoverPath, coverPath, fs::copy_options::skip_existing);
            }

            if (_ffmpegPath.empty()) {
                trackPath.replace_extension(playback->ActualExt);
                fs::rename(playback->FileName, trackPath);
            } else {
                auto& fmt = _config["outputFormat"];
                auto convExt = fmt["ext"].get<std::string>();
                auto convArgs = fmt["args"].get<std::string>();

                trackPath.replace_extension(convExt.empty() ? playback->ActualExt : convExt);
                InvokeFFmpeg(playback->FileName, tmpCoverPath, trackPath, convArgs, meta);
            }
            fs::remove(playback->FileName);
            SendTrackStatus(trackUri, "DONE", "", trackPath);
            LogInfo("Done saving track {}", trackName);
        } catch (std::exception& ex) {
            LogError("Failed to save track {}: {}", trackName, ex.what());
            SendTrackStatus(trackUri, "ERROR", ex.what());
        }
    }
    void SearchPathTree(json& results, const json& node, const fs::path& currPath, const fs::path& relativeTo = {}, bool currPathExists = false)
    {
        if (!currPath.empty() && !(currPathExists || fs::exists(currPath))) return;

        auto& children = node["children"];
        if (children.empty() && node.contains("id")) {
            std::string id = node["id"];
            results[id] = {
                { "path", relativeTo.empty() ? currPath : fs::proximate(currPath, relativeTo) },
                { "status", "DONE" }
            };
            return;
        }
        std::vector<std::tuple<const json*, std::regex, int>> regexChildren;

        for (auto& child : children) {
            std::string pattern = child["pattern"];
            if (child.value("literal", false)) {
                SearchPathTree(results, child, currPath / fs::u8path(pattern), relativeTo);
            } else {
                std::regex regex(pattern, std::regex::ECMAScript | std::regex::icase);
                regexChildren.emplace_back(&child, regex, child.value("maxDepth", 1));
            }
        }
        if (regexChildren.empty()) return;

        //BFS up to maxDepth to catch patterns like "album(\/CD 1)?"
        std::deque<std::pair<fs::path, int>> pending;
        pending.emplace_back(currPath, 0);

        for (; !pending.empty(); pending.pop_front()) {
            auto& [origin, depth] = pending.front();

            for (auto& entry : fs::directory_iterator(origin)) {
                auto name = Utils::PathToUtf(fs::relative(entry.path(), currPath));

                for (auto& [node, pattern, maxDepth] : regexChildren) {
                    if (depth < maxDepth && std::regex_match(name, pattern)) {
                        SearchPathTree(results, *node, entry.path(), relativeTo, true);
                    }
                    if (entry.is_directory() && depth + 1 < maxDepth) {
                        pending.emplace_back(entry.path(), depth + 1);
                    }
                }
            }
        }
    }
    void SendTrackStatus(const std::string& uri, const std::string& status, const std::string& msg = "", const fs::path& path = "")
    {
        json obj = {
            { "status", status },
            { "message", msg },
            { "path", Utils::PathToUtf(path) }
        };
        json content;
        
        if (uri.starts_with("spotify:")) {
            content["results"] = { { uri, obj } };
        } else {
            content = obj;
            content["playbackId"] = uri;
        }
        _ctrlSv.Broadcast(MessageType::DOWNLOAD_STATUS, content);
    }
    
    void InvokeFFmpeg(const fs::path& path, const fs::path& coverPath, const fs::path& outPath, const std::string& extraArgs, const json& meta)
    {
        //TODO: fix 32k char command line limit for lyrics
        if (fs::exists(outPath)) {
            LogInfo("File {} already exists. Skipping conversion.", Utils::PathToUtf(outPath.filename()));
            return;
        }
        ProcessBuilder proc;
        proc.SetExePath(_ffmpegPath);
        proc.SetWorkDir(_dataDir);

        proc.AddArgs("-y -loglevel warning -i");
        proc.AddArgPath(path);

        if (_config.value("embedCoverArt", true)) {
            auto outExt = outPath.extension();
            if (outExt == ".ogg" || outExt == ".opus") {
                //ffmpeg can't create OGGs with covers directly, we need to manually
                //create a flac metadata block and attach it instead.
                proc.AddArg("-i");
                proc.AddArgPath(CreateCoverMetaBlock(coverPath));
                proc.AddArgs("-map_metadata 1");
            } else {
                proc.AddArg("-i");
                proc.AddArgPath(coverPath);
                proc.AddArgs("-map 0:0 -map 1:0");
            }
        }
        if (!extraArgs.empty()) {
            proc.AddArgs(extraArgs);
        }
        for (auto& [k, v] : meta.items()) {
            proc.AddArg("-metadata");
            proc.AddArg(k + "=" + v.get<std::string>());
        }
        proc.AddArgPath(outPath);

        LogTrace("  ffmpeg args: {}", proc.ToString());
        int exitCode = proc.Start(true, true);
        if (exitCode != 0) {
            throw std::runtime_error("FFmpeg exited with code " + std::to_string(exitCode) + ". Check log for details.");
        }
    }
    fs::path CreateCoverMetaBlock(const fs::path& coverPath)
    {
        fs::path outPath = coverPath;
        outPath.replace_extension(".ffmd");

        if (!fs::exists(outPath)) {
            //https://superuser.com/a/169158
            //https://xiph.org/flac/format.html#metadata_block_picture
            std::string data;
            data.append("\x00\x00\x00\x03", 4);                 //Type: Front cover
            data.append("\x00\x00\x00\x0A" "image/jpeg", 14);   //Mime type
            data.append("\x00\x00\x00\x0B" "Front Cover", 15);  //Description
            data.append("\x00\x00\x00\x00", 4);                 //Width
            data.append("\x00\x00\x00\x00", 4);                 //Height
            data.append("\x00\x00\x00\x00", 4);                 //Bits per pixel
            data.append("\x00\x00\x00\x00", 4);                 //Palette size

            std::ifstream imageFs(coverPath, std::ios::binary);
            imageFs.seekg(0, std::ios::end);
            int imageLen = (int)imageFs.tellg();

            //Data length
            data.append({
                (char)(imageLen >> 24),
                (char)(imageLen >> 16),
                (char)(imageLen >> 8),
                (char)(imageLen >> 0),
            });
            //Data
            imageFs.seekg(0);
            data.append(std::istreambuf_iterator<char>(imageFs), std::istreambuf_iterator<char>());

            //Write out the encoded ffmpeg block
            std::ofstream outs(outPath, std::ios::binary);
            outs << ";FFMETADATA1\n";
            outs << "METADATA_BLOCK_PICTURE=";
            outs << Utils::EncodeBase64(data) << "\n";
        }
        return outPath;
    }

    fs::path FindFFmpegPath()
    {
        //Try Soggfy/ffmpeg/ffmpeg.exe
        auto path = _dataDir / "ffmpeg/ffmpeg.exe";
        if (fs::exists(path)) {
            return path;
        }
        //Try %PATH%
        std::wstring envPath;
        DWORD envPathLen = SearchPath(NULL, L"ffmpeg.exe", NULL, 0, envPath.data(), NULL);
        if (envPathLen != 0) {
            envPath.resize(envPathLen - 1);
            SearchPath(NULL, L"ffmpeg.exe", NULL, envPath.capacity(), envPath.data(), NULL);

            return fs::path(envPath);
        }
        return {};
    }
    
    std::shared_ptr<PlaybackInfo> GetPlayback(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto itr = _playbacks.find(id);
        return itr != _playbacks.end() ? itr->second : nullptr;
    }
    void CreatePlayback(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_playbacks.contains(id)) {
            //This only seems to happen when skipping tracks with ~1s of playback left
            auto& pb = _playbacks.find(id)->second;
            auto pos = pb->FileStream.tellp();
            LogWarn("CreatePlayback() called for a playback that already exists. id={}, file pos={}", id, (int64_t)pos);
            if (pos != 0) {
                //Enter wtf mode if something was written to the dump file
                DiscardTrack(*pb, "CreatePlayback() called for a playback that already exists");
            }
            return;
        }
        auto filename = MakeTempPath("playback_" + id + ".dat");
        auto pb = std::make_shared<PlaybackInfo>();
        pb->Id = id;
        pb->FileName = filename;
        pb->FileStream.open(filename, std::ios::binary);
        pb->Status = std::make_pair("IN_PROGRESS", "Downloading...");

        _playbacks.emplace(id, pb);
    }
    void RemovePlayback(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _playbacks.erase(id);
    }

    fs::path MakeTempPath(const std::string& filename)
    {
        fs::create_directories(_dataDir / "temp");
        return _dataDir / "temp" / filename;
    }
};

std::unique_ptr<StateManager> StateManager::New(const fs::path& dataDir)
{
    return std::make_unique<StateManagerImpl>(dataDir);
}