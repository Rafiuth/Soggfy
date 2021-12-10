#include "pch.h"

#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Http.h"
#include "Utils/Utils.h"

namespace fs = std::filesystem;
using namespace nlohmann;

struct PlaybackInfo
{
    std::string TrackId;
    bool WasSeeked = false;
    int StartPos = 0;
    bool Closed = false;
    bool PlayedToEnd = false; //whether closeReason == "trackdone"

    std::string ActualExt;
    fs::path FileName;
    std::ofstream FileStream;
};

struct TrackMetadata
{
    //Properties that can be passed directly to ffmpeg
    //https://wiki.multimedia.cx/index.php/FFmpeg_Metadata
    std::unordered_map<std::string, std::string> Props;
    std::string CoverUrl;
    std::string Type; //either "track" or "podcast"
    json RawData;

    std::string GetName() const
    {
        return Props.at("album_artist") + " - " + Props.at("title");
    }
};

struct StateManagerImpl : public StateManager
{
    fs::path _dataDir;
    std::string _accessToken;

    std::unordered_map<std::string, std::shared_ptr<PlaybackInfo>> _playbacks;

    fs::path _ffmpegPath;
    json _config;

    StateManagerImpl(const fs::path& dataDir) :
        _dataDir(dataDir)
    {
        std::ifstream configFile(dataDir / "config.json");
        if (configFile.good()) {
            _config = json::parse(configFile, nullptr, true, true);
        }
        //add missing values
        _config.emplace("track_path_fmt", "%userprofile%/Music/Soggfy/{artist_name}/{album_name}/{track_num}. {track_name}.ogg");
        _config.emplace("cover_path_fmt", "%userprofile%/Music/Soggfy/{artist_name}/{album_name}/cover.jpg");
        _config.emplace("podcast_path_fmt", "%userprofile%/Music/SoggfyPodcasts/{artist_name}/{album_name}/{track_name}.ogg");
        _config.emplace("podcast_cover_path_fmt", "%userprofile%/Music/SoggfyPodcasts/{artist_name}/{album_name}/cover.jpg");
        _config.emplace("convert_keep_ogg", false);

        //delete old temp files
        std::error_code deleteError;
        fs::remove_all(dataDir / "temp", deleteError);

        _ffmpegPath = FindFFmpegPath();
        if (_ffmpegPath.empty()) {
            LogWarn("FFmpeg binaries were not found, downloaded files will not be tagged nor converted. Run DownloadFFmpeg.ps1 to fix this.");
        }
    }

    void OnTrackCreated(const std::string& playbackId, const std::string& trackId)
    {
        LogDebug("TrackCreated: id={} playId={}", trackId, playbackId);

        if (_playbacks.contains(playbackId)) {
            LogWarn("Playback with the same id already exists, overwriting.");
        }
        auto info = std::make_shared<PlaybackInfo>();
        info->TrackId = trackId;
        info->FileName = _dataDir / "temp" / ("playback_" + playbackId + ".dat");
        fs::create_directories(info->FileName.parent_path());
        info->FileStream.open(info->FileName, std::ios::binary);

        _playbacks[playbackId] = info;
    }
    void OnTrackOpened(const std::string& playbackId, int positionMs)
    {
        auto itr = _playbacks.find(playbackId);
        if (itr == _playbacks.end()) {
            LogWarn("Track opened for unknown playback, ignoring.");
            return;
        }
        auto& playback = itr->second;
        playback->StartPos = positionMs;

        LogInfo("New track detected: {}", playback->TrackId);
    }
    void OnTrackClosed(const std::string& playbackId, const std::string& reason)
    {
        LogDebug("TrackClosed: playId={} reason={}", playbackId, reason);
        
        auto itr = _playbacks.find(playbackId);
        if (itr != _playbacks.end()) {
            auto& playback = itr->second;

            playback->Closed = true;
            playback->PlayedToEnd = reason == "trackdone";

            FlushClosedTracks();
        }
    }
    void OnTrackSeeked(const std::string& playbackId)
    {
#if !_DEBUG
        auto itr = _playbacks.find(playbackId);
        if (itr != _playbacks.end()) {
            itr->second->WasSeeked = true;
        }
#endif
    }

    void ReceiveAudioData(const std::string& playbackId, const char* data, int length)
    {
        auto itr = _playbacks.find(playbackId);
        if (itr == _playbacks.end()) return;

        auto& playback = itr->second;
        auto& fs = playback->FileStream;

        if (!fs.is_open()) {
            LogWarn("Received audio data after playback ended (this shouldn't happen). Last track could've been truncated.");
            return;
        }
        if (fs.tellp() == 0) {
            if (memcmp(data, "OggS", 4) == 0) {
                //skip spotify's custom ogg page (it sets the EOS flag, causing players to think the file is corrupt)
                auto nextPage = Utils::FindPosition(data + 4, length - 4, "OggS", 4);

                if (nextPage < 0) {
                    //this may happen if the buffer is too small.
                    LogWarn("Could not skip Spotify's custom OGG page. Downloaded file might be broken. ({})", playback->TrackId);
                } else {
                    length -= nextPage - data;
                    data = nextPage;
                }
                playback->ActualExt = "ogg";
            } else if (memcmp(data, "ID3", 3) == 0 || (data[0] == 0xFF && (data[1] & 0xF0) == 0xF0)) {
                playback->ActualExt = "mp3";
            } else {
                LogWarn("Unrecognized audio data type. Downloaded file might be broken. ({})", playback->TrackId);
            }
        }
        fs.write(data, length);
    }

    void UpdateAccToken(const std::string& token)
    {
        LogDebug("Update access token: {}", token);
        _accessToken = token;
    }
    void Shutdown()
    {
        for (auto& [id, playback] : _playbacks) {
            playback->FileStream.close();
        }
    }
    
    void FlushClosedTracks()
    {
        if (_accessToken.empty()) {
            LogDebug("FlushTracks: access token not available yet, deferring...");
            return;
        }
        //Iterate and remove finished playbacks
        auto numFlushed = std::erase_if(_playbacks, [&](const auto& elem) {
            auto& playback = elem.second;
            if (!playback->Closed) return false;

            if (playback->StartPos == 0 && playback->PlayedToEnd && !playback->WasSeeked) {
                playback->FileStream.close();

                std::thread t(&StateManagerImpl::TagAndMoveToOutput, this, playback);
                t.detach();
            } else {
                LogInfo("Discarding track {} because it was not fully played.", playback->TrackId);
            }
            return true;
        });
        LogDebug("FlushTracks: flushed {} tracks. Alive playbacks: {}.", numFlushed, _playbacks.size());
    }

    void TagAndMoveToOutput(std::shared_ptr<PlaybackInfo> playback)
    {
        auto& trackId = playback->TrackId;

        try {
            auto meta = FetchTrackMetadata(trackId);

            LogInfo("Saving track {}", trackId);
            LogInfo("  title: {}", meta.GetName());
            LogDebug("  stream: {}", Utils::PathToUtf(playback->FileName));
            LogDebug("  meta: {}", meta.RawData.dump());

            auto trackPath = RenderTrackPath(meta.Type + "_path_fmt", meta);
            auto coverPath = RenderTrackPath(meta.Type + "_cover_path_fmt", meta);

            auto tmpCoverPath = _dataDir / "temp" / (Utils::RemoveInvalidPathChars(meta.CoverUrl) + ".jpg");

            if (!fs::exists(tmpCoverPath)) {
                DownloadFile(tmpCoverPath, meta.CoverUrl);
            }
            if (!coverPath.empty() && !fs::exists(coverPath)) {
                fs::create_directories(coverPath.parent_path());
                fs::copy_file(tmpCoverPath, coverPath);
            }

            fs::create_directories(trackPath.parent_path());
            if (_ffmpegPath.empty()) {
                fs::rename(playback->FileName, trackPath);
                return;
            }
            auto convFmt = _config["convert_to"].get<std::string>();

            if (!convFmt.empty()) {
                auto& fmt = _config["formats"][convFmt];

                auto convExt = fmt["ext"].get<std::string>();
                auto convArgs = fmt["args"].get<std::string>();

                fs::path outPath = trackPath;
                outPath.replace_extension(convExt);

                InvokeFFmpeg(playback->FileName, tmpCoverPath, outPath, convArgs, meta);
            }
            if (convFmt.empty() || _config["convert_keep_ogg"].get<bool>()) {
                fs::path outPath = trackPath;
                outPath.replace_extension(playback->ActualExt);

                InvokeFFmpeg(playback->FileName, tmpCoverPath, outPath, "-c copy", meta);
            }
            fs::remove(playback->FileName);
        } catch (std::exception& ex) {
            LogError("Failed to save track {}: {}", trackId, ex.what());
        }
    }
    void InvokeFFmpeg(const fs::path& path, const fs::path& coverPath, const fs::path& outPath, const std::string& extraArgs, const TrackMetadata& meta)
    {
        if (fs::exists(outPath)) {
            LogDebug("File {} already exists. Skipping conversion.", Utils::PathToUtf(outPath));
            return;
        }
        std::vector<std::wstring> args;
        args.push_back(L"-i");
        args.push_back(path.wstring());

        if (outPath.extension() == ".ogg" || outPath.extension() == ".opus") {
            //ffmpeg can't create OGGs with covers directly, we need to manually
            //create a flac metadata block and attach it instead.
            args.push_back(L"-i");
            args.push_back(CreateCoverMetaBlock(coverPath).wstring());
            Utils::SplitCommandLine(L"-map_metadata 1", args);
        } else {
            args.push_back(L"-i");
            args.push_back(coverPath.wstring());
            Utils::SplitCommandLine(L"-map 0:0 -map 1:0", args);
        }
        if (!extraArgs.empty()) {
            Utils::SplitCommandLine(Utils::StrUtfToWide(extraArgs), args);
        }
        for (auto& [k, v] : meta.Props) {
            args.push_back(L"-metadata");
            args.push_back(Utils::StrUtfToWide(k + "=" + v));
        };
        args.push_back(outPath.wstring());
        args.push_back(L"-y");

        LogDebug("Converting {}...", meta.GetName());
        LogDebug("  args: {}", Utils::StrWideToUtf(Utils::CreateCommandLine(args)));

        uint32_t exitCode = Utils::StartProcess(_ffmpegPath, args, _dataDir, true);
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
            outs << EncodeBase64(data.data(), data.size()) << "\n";
        }
        return outPath;
    }
    std::string EncodeBase64(const char* data, size_t len)
    {
        //https://stackoverflow.com/a/6782480
        const char ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const int MOD_TABLE[] = { 0, 2, 1 };

        size_t outLen = 4 * ((len + 2) / 3);
        std::string res(outLen, '\0');

        for (size_t i = 0, j = 0; i < len;) {
            uint8_t octet_a = i < len ? (uint8_t)data[i++] : 0;
            uint8_t octet_b = i < len ? (uint8_t)data[i++] : 0;
            uint8_t octet_c = i < len ? (uint8_t)data[i++] : 0;

            uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

            res[j++] = ALPHA[(triple >> 3 * 6) & 63];
            res[j++] = ALPHA[(triple >> 2 * 6) & 63];
            res[j++] = ALPHA[(triple >> 1 * 6) & 63];
            res[j++] = ALPHA[(triple >> 0 * 6) & 63];
        }
        for (int i = 0; i < MOD_TABLE[len % 3]; i++) {
            res[outLen - 1 - i] = '=';
        }
        return res;
    }

    TrackMetadata FetchTrackMetadata(const std::string& trackId)
    {
        std::string path;

        if (trackId.starts_with("spotify:track:")) {
            path = "/v1/tracks/" + trackId.substr(14);
        } else if (trackId.starts_with("spotify:episode:")) {
            path = "/v1/episodes/" + trackId.substr(16);
        } else {
            throw std::runtime_error("Unknown track type (id=" + trackId + ")");
        }
        Http::Request req;
        req.Url = "https://api.spotify.com" + path;
        req.Headers.emplace("Accept", "application/json");
        req.Headers.emplace("Authorization", "Bearer " + _accessToken);
        //FIXME: is it good to fake user-agent headers?

        auto resp = Http::Fetch(req);
        auto& rawJson = resp.Content;
        auto json = json::parse(rawJson.begin(), rawJson.end());

        TrackMetadata md;
        md.RawData = json;

        if (json["type"] == "track") {
            std::string date = json["album"]["release_date"];
            std::string artists;
            for (auto& artist : json["artists"]) {
                if (artists.size() != 0) artists += ", ";
                artists += artist["name"];
            }
            md.Type = "track";
            md.Props["title"] = json["name"];
            md.Props["artist"] = artists;
            md.Props["album_artist"] = json["artists"][0]["name"];
            md.Props["album"] = json["album"]["name"];
            md.Props["releasetype"] = json["album"]["album_type"];
            md.Props["date"] = date;
            md.Props["year"] = date.substr(0, 4);
            md.Props["track"] = std::to_string(json["track_number"].get<int>());
            md.Props["totaltracks"] = std::to_string(json["album"]["total_tracks"].get<int>());
            md.Props["disc"] = std::to_string(json["disc_number"].get<int>());

            if (json["external_ids"].contains("isrc")) {
                md.Props["isrc"] = json["external_ids"]["isrc"];
            }
            md.CoverUrl = json["album"]["images"][0]["url"];
        } else if (json["type"] == "episode") {
            std::string date = json["release_date"];

            md.Type = "podcast";
            md.Props["title"] = json["name"];
            md.Props["publisher"] = json["show"]["publisher"];
            md.Props["album_artist"] = json["show"]["publisher"];
            md.Props["album"] = json["show"]["name"];
            md.Props["date"] = date;
            md.Props["year"] = date.substr(0, 4);
            md.Props["totaltracks"] = std::to_string(json["show"]["total_episodes"].get<int>());
            md.Props["description"] = json["description"];

            md.CoverUrl = json["images"][0]["url"];
        } else {
            throw std::runtime_error("Don't know how to parse track metadata (type=" + json["type"].get<std::string>() + ")");
        }
        return md;
    }
    void DownloadFile(const fs::path& path, const std::string& url)
    {
        Http::Request req;
        req.Url = url;
        req.Headers.emplace("Accept", "*/*");

        auto resp = Http::Fetch(req);

        std::ofstream file(path, std::ios::out | std::ios::binary);
        file.write((char*)resp.Content.data(), resp.Content.size());
    }

    fs::path RenderTrackPath(const std::string& fmtKey, const TrackMetadata& metadata)
    {
        std::string fmt = _config[fmtKey].get<std::string>();

        fmt = Utils::StrRegexReplace(fmt, std::regex("\\{(.+?)\\}"), [&](std::smatch const& m) {
            auto& val = metadata.Props.at(m.str(1));
            return Utils::RemoveInvalidPathChars(val);
        });
        return fs::u8path(Utils::ExpandEnvVars(fmt));
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
};

std::unique_ptr<StateManager> StateManager::New(const fs::path& dataDir)
{
    return std::make_unique<StateManagerImpl>(dataDir);
}