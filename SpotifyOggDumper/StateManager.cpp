#include "pch.h"

#include "StateManager.h"
#include "ControlServer.h"
#include "JsInjector.h"
#include "Utils/Log.h"
#include "Utils/Http.h"
#include "Utils/Utils.h"

struct PlaybackInfo
{
    std::string Id;
    fs::path FileName;
    std::ofstream FileStream;
    std::string ActualExt;
    bool Discard = false;
};
struct TrackMetadata
{
    std::string Type; //either "track" or "podcast"
    std::string TrackUri;
    //Properties that can be passed directly to ffmpeg
    //https://wiki.multimedia.cx/index.php/FFmpeg_Metadata
    std::unordered_map<std::string, std::string> Props;
    std::unordered_map<std::string, std::string> PathVars;
    std::string Lyrics; //Raw LRC bytes
    std::string LyricsExt; //either "lrc" or "txt"
    std::string CoverArtId;
    std::string CoverArtData; //Raw jpg bytes

    std::string GetName() const
    {
        return Props.at("album_artist") + " - " + Props.at("title");
    }
};

struct StateManagerImpl : public StateManager
{
    fs::path _dataDir;

    std::unordered_map<std::string, std::shared_ptr<PlaybackInfo>> _playbacks;

    std::mutex _mutex;

    fs::path _ffmpegPath;
    json _config;

    ControlServer _ctrlSv;

    StateManagerImpl(const fs::path& dataDir) :
        _dataDir(dataDir),
        _ctrlSv(
            [this](auto conn, auto& msg) { HandleMessage(conn, msg); }
        )
    {
        std::ifstream configFile(dataDir / "config.json");
        if (configFile.good()) {
            _config = json::parse(configFile, nullptr, true, true);
        }
        //delete old temp files
        std::error_code deleteError;
        fs::remove_all(dataDir / "temp", deleteError);

        _ffmpegPath = FindFFmpegPath();
        if (_ffmpegPath.empty()) {
            LogWarn("FFmpeg binaries were not found, downloaded files won't be tagged nor converted. Run DownloadFFmpeg.ps1 to fix this.");
        }
    }

    void RunControlServer()
    {
        _ctrlSv.Run();
    }

    void HandleMessage(Connection* conn, const Message& msg)
    {
        auto& content = msg.Content;

        LogTrace("ControlMessage: {} {} + {} bytes", (int)msg.Type, msg.Content.dump(), msg.BinaryContent.size());

        switch (msg.Type) {
            case MessageType::SERVER_OPEN: {
                LogInfo("Injecting JS into CEF...");
                std::ifstream srcJsFile(_dataDir / "Soggfy.js", std::ios::binary);
                std::string srcJs(std::istreambuf_iterator<char>(srcJsFile), {});

                Utils::Replace(srcJs, "ws://127.0.0.1:28653/sgf_ctrl", msg.Content["addr"]);
                Utils::Replace(srcJs, "IS_INJECTED = false", "IS_INJECTED = true");
#ifdef _DEBUG
                Utils::Replace(srcJs, "IS_DEBUG = false", "IS_DEBUG = true");
#endif
                JsInjector::Inject(srcJs);
                LogInfo("Ready");
                break;
            }
            case MessageType::HELLO: {
                conn->Send({ MessageType::SYNC_CONFIG, _config });
                break;
            }
            case MessageType::BYE: {
                break;
            }
            case MessageType::TRACK_META: {
                std::string playbackId = content["playbackId"];
                auto playback = GetPlayback(playbackId, false);

                TrackMetadata meta = {
                    .Type       = content["type"],
                    .TrackUri   = content["trackUri"],
                    .Props      = content["metadata"],
                    .PathVars   = content["pathVars"],
                    .Lyrics     = content["lyrics"],
                    .LyricsExt  = content["lyricsExt"],
                    .CoverArtId = content["coverArtId"],
                    .CoverArtData = msg.BinaryContent
                };
                std::thread(&StateManagerImpl::SaveTrack, this, playback, meta).detach();
                RemovePlayback(playbackId);
                break;
            }
            case MessageType::SYNC_CONFIG: {
                for (auto& [key, val] : content.items()) {
                    _config[key] = val;
                }
                std::ofstream(_dataDir / "config.json") << _config.dump(4);
                break;
            }
            default: {
                throw std::runtime_error("Unexpected message (type=" + std::to_string((int)msg.Type) + ")");
            }
        }
    }

    void ReceiveAudioData(const std::string& playbackId, const char* data, int length)
    {
        auto playback = GetPlayback(playbackId, true);
        auto& fs = playback->FileStream;

        if (playback->Discard) return;

        if (!fs.is_open()) {
            LogWarn("Received audio data after playback ended (this shouldn't happen). Last track could've been truncated.");
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
    void OnTrackDone(const std::string& playbackId)
    {
        auto playback = GetPlayback(playbackId, false);
        playback->FileStream.close();

        if (playback->Discard) {
            RemovePlayback(playbackId);
            return;
        }
        if (playback->ActualExt.empty()) {
            LogWarn("Unrecognized audio codec in playback {}, aborting download. This is a bug, please report.", playbackId);
            return;
        }
        json content = {
            { "playbackId", playbackId }
        };
        _ctrlSv.Broadcast({ MessageType::REQ_TRACK_META, content });
    }
    void DiscardTrack(const std::string& playbackId)
    {
        auto playback = GetPlayback(playbackId, true);
        playback->Discard = true;
    }
    bool OverridePlaybackSpeed(double& speed)
    {
        double newSpeed = _config.value("playbackSpeed", 0.0);
        if (newSpeed > 0) {
            speed = newSpeed;
            return true;
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
    
    void SaveTrack(std::shared_ptr<PlaybackInfo> playback, TrackMetadata meta)
    {
        try {
            LogInfo("Saving track {}", meta.GetName());
            LogDebug("  stream: {}", playback->FileName.filename().string());

            fs::path trackPath = RenderTrackPath(_config["savePaths"][meta.Type]["audio"], meta);
            fs::path coverPath = RenderTrackPath(_config["savePaths"][meta.Type]["cover"], meta);
            fs::path tmpCoverPath = MakeTempPath(meta.CoverArtId + ".jpg");

            //always cache cover art
            if (!fs::exists(tmpCoverPath)) {
                std::ofstream(tmpCoverPath, std::ios::binary) << meta.CoverArtData;
            }
            fs::create_directories(trackPath.parent_path());

            if (!coverPath.empty()) {
                fs::copy_file(tmpCoverPath, coverPath, fs::copy_options::skip_existing);
            }
            if (_ffmpegPath.empty()) {
                fs::path outPath = trackPath;
                outPath.replace_extension(playback->ActualExt);
                fs::rename(playback->FileName, outPath);
            } else {
                auto& fmt = _config["outputFormat"];
                auto convExt = fmt["ext"].get<std::string>();
                auto convArgs = fmt["args"].get<std::string>();

                fs::path outPath = trackPath;
                outPath.replace_extension(convExt.empty() ? playback->ActualExt : convExt);
                InvokeFFmpeg(playback->FileName, tmpCoverPath, outPath, convArgs, meta);
            }
            if (!meta.Lyrics.empty()) {
                fs::path lrcPath = trackPath;
                lrcPath.replace_extension(meta.LyricsExt);
                std::ofstream(lrcPath, std::ios::binary) << meta.Lyrics;
            }
            fs::remove(playback->FileName);
        } catch (std::exception& ex) {
            LogError("Failed to save track {}: {}", meta.GetName(), ex.what());
        }
    }
    void InvokeFFmpeg(const fs::path& path, const fs::path& coverPath, const fs::path& outPath, const std::string& extraArgs, const TrackMetadata& meta)
    {
        //TODO: fix 32k char command line limit for lyrics
        if (fs::exists(outPath)) {
            LogDebug("File {} already exists. Skipping conversion.", outPath.filename().string());
            return;
        }
        ProcessBuilder proc;
        proc.SetExePath(_ffmpegPath);
        proc.SetWorkDir(_dataDir);

        proc.AddArg("-i");
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
        for (auto& [k, v] : meta.Props) {
            proc.AddArg("-metadata");
            proc.AddArg(k + "=" + v);
        };
        proc.AddArgPath(outPath);
        proc.AddArg("-y");

        LogDebug("Converting {}...", meta.GetName());
        LogDebug("  args: {}", proc.ToString());

        int exitCode = proc.Start(true);
        if (exitCode != 0) {
            throw std::runtime_error("Conversion failed. (ffmpeg returned " + std::to_string(exitCode) + ")");
        }
        LogInfo("Done converting {}.", meta.GetName());
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

    fs::path RenderTrackPath(const std::string& format, const TrackMetadata& metadata)
    {
        std::unordered_map<std::string, std::string> vars;
        vars.emplace("user_home", Utils::GetHomeDirectory());
        for (auto& [k, v] : metadata.PathVars) {
            vars.emplace(k, Utils::RemoveInvalidPathChars(v));
        }
        std::string path = Utils::RegexReplace(format, std::regex("\\{(.+?)\\}"), [&](auto& m) {
            auto varName = m.str(1);
            auto itr = vars.find(varName);
            return itr != vars.end() ? itr->second : "_";
        });
        return fs::u8path(path);
    }
    
    std::shared_ptr<PlaybackInfo> GetPlayback(const std::string& id, int createIfNotFound)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto itr = _playbacks.find(id);
        if (itr != _playbacks.end()) {
            return itr->second;
        }
        if (!createIfNotFound) {
            throw std::runtime_error("Unknown playback " + id);
        }
        auto filename = MakeTempPath("playback_" + id + ".dat");
        auto pb = std::make_shared<PlaybackInfo>();
        pb->Id = id;
        pb->FileName = filename;
        pb->FileStream.open(filename, std::ios::binary);

        _playbacks.emplace(id, pb);
        return pb;
    }
    void RemovePlayback(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _playbacks.erase(id);
    }

    fs::path MakeTempPath(const std::string& filename)
    {
        fs::create_directories(_dataDir / "temp");
        return _dataDir / "temp" / Utils::RemoveInvalidPathChars(filename);
    }
};

std::unique_ptr<StateManager> StateManager::New(const fs::path& dataDir)
{
    return std::make_unique<StateManagerImpl>(dataDir);
}