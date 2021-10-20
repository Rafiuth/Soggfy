#include "pch.h"

#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Http.h"
#include "Utils/Utils.h"

namespace fs = std::filesystem;
using namespace nlohmann;

struct PlaybackInfo;

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

enum class TrackType
{
    Music, PodcastEpisode
};
struct TrackMetadata
{
    TrackType Type;

    std::string TrackName;
    std::vector<std::string> Artists;
    std::string AlbumName;
    std::string AlbumType;
    std::string ReleaseDate; //Format: Year-Month-Day
    std::string CoverUrl;
    int TrackNum = 0;
    int DiscNum = 0;
    int TotalTracks = 0;

    //Music only
    std::string ISRC;

    //Podcast only
    std::string Description;

    json RawData;

    int GetReleaseYear() const
    {
        return std::stoi(ReleaseDate.substr(0, ReleaseDate.find('-')));
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

        if (_config.contains("convert_args") && (_ffmpegPath = FindFFmpegPath()).empty()) {
            LogWarn("config.json has 'convert_args' field but ffmpeg binaries were not found. Run DownloadFFmpeg.ps1 to fix this.");
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
        info->FileStream.open(info->FileName, std::ios::out | std::ios::binary);

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

        auto playback = itr->second;
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
                    LogWarn("Could not skip Spotify's custom OGG page. Downloaded file might be broken. ({})", playbackId);
                } else {
                    length -= nextPage - data;
                    data = nextPage;
                }
                playback->ActualExt = "ogg";
            } else if (memcmp(data, "ID3", 3) == 0 || (data[0] == 0xFF && (data[1] & 0xF0) == 0xF0)) {
                playback->ActualExt = "mp3";
            } else {
                LogWarn("Unrecognized audio data type. Downloaded file might be broken. ({})", playbackId);
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
            LogInfo("  title: {} - {}", meta.Artists[0], meta.TrackName);
            LogDebug("  stream: {}", playback->FileName.filename().string());
            LogDebug("  meta: {}", meta.RawData.dump());

            auto pathKeys = meta.Type == TrackType::Music
                ? std::make_tuple("track_path_fmt", "cover_path_fmt")
                : std::make_tuple("podcast_path_fmt", "podcast_cover_path_fmt");

            auto trackPath = RenderTrackPath(std::get<0>(pathKeys), meta);
            auto coverPath = RenderTrackPath(std::get<1>(pathKeys), meta);

            auto tmpCoverPath = _dataDir / "temp" / (Utils::RemoveInvalidPathChars(meta.CoverUrl) + ".jpg");

            if (!fs::exists(tmpCoverPath)) {
                DownloadFile(tmpCoverPath, meta.CoverUrl);
            }
            if (!coverPath.empty() && !fs::exists(coverPath)) {
                fs::create_directories(coverPath.parent_path());
                fs::copy_file(tmpCoverPath, coverPath);
            }
            if (playback->ActualExt == "ogg") {
                WriteTags(playback->FileName, tmpCoverPath, meta);
            }
            CreateFinalTrack(playback->FileName, trackPath, meta);
        } catch (std::exception& ex) {
            LogError("Failed to save track {}: {}", trackId, ex.what());
        }
    }

    void CreateFinalTrack(const fs::path& path, const fs::path& dstPath, const TrackMetadata& meta)
    {
        fs::create_directories(dstPath.parent_path());

        auto& convArgs = _config["convert_args"];
        auto& convExt = _config["convert_ext"];

        if (convArgs.is_string() && !_ffmpegPath.empty()) {
            fs::path outPath = dstPath;
            outPath.replace_extension(convExt.get<std::string>());

            if (fs::exists(outPath)) return;

            std::wstring args = std::format(L"-i \"{}\" {} \"{}\"", 
                path.wstring(),
                Utils::StrUtfToWide(convArgs.get<std::string>()),
                outPath.wstring()
            );
            LogInfo("Converting {} - {}...", meta.Artists[0], meta.TrackName);
            LogDebug("  args: {}", Utils::StrWideToUtf(args));

            uint32_t exitCode = Utils::StartProcess(_ffmpegPath, args, _dataDir, true);
            if (exitCode != 0) {
                throw std::runtime_error("Conversion failed. (ffmpeg returned " + std::to_string(exitCode) + ")");
            }
            LogInfo("...done");

            if (_config.value("convert_keep_ogg", false)) {
                fs::rename(path, dstPath);
            } else {
                fs::remove(path);
            }
        } else {
            fs::rename(path, dstPath);
        }
    }

    void WriteTags(const fs::path& path, const fs::path& coverPath, const TrackMetadata& meta)
    {
        std::ifstream coverFile(coverPath, std::ios::binary);
        auto coverData = std::vector<char>(std::istreambuf_iterator<char>(coverFile), std::istreambuf_iterator<char>());

        std::string artists;
        for (auto& artistName : meta.Artists) {
            if (artists.size() > 0) artists += ", ";
            artists += artistName;
        }

        TagLib::Ogg::Vorbis::File ogg(path.wstring().c_str());
        auto* tag = ogg.tag();
        
        auto coverArt = new TagLib::FLAC::Picture();
        coverArt->setType(TagLib::FLAC::Picture::Type::FrontCover);
        coverArt->setMimeType("image/jpeg");
        coverArt->setDescription("Front Cover");
        coverArt->setData(TagLib::ByteVector(coverData.data(), (uint32_t)coverData.size()));

        tag->addPicture(coverArt);
        tag->setTitle(TagLib::String(meta.TrackName, TagLib::String::UTF8));
        tag->setArtist(TagLib::String(artists, TagLib::String::UTF8));
        tag->setAlbum(TagLib::String(meta.AlbumName, TagLib::String::UTF8));
        tag->setTrack(meta.TrackNum);
        tag->setYear(meta.GetReleaseYear());

        tag->addField("DATE", meta.ReleaseDate);
        tag->addField("TOTALTRACKS", std::to_string(meta.TotalTracks));
        
        if (meta.Type == TrackType::Music) {
            tag->addField("DISCNUMBER", std::to_string(meta.DiscNum));
            tag->addField("ISRC", meta.ISRC);
            tag->addField("RELEASETYPE", meta.AlbumType);
        } else if (meta.Type == TrackType::PodcastEpisode) {
            tag->addField("DESCRIPTION", TagLib::String(meta.Description, TagLib::String::UTF8));
        }
        ogg.save();
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
            md.Type = TrackType::Music;
            md.TrackName = json["name"];
            for (auto& artist : json["artists"]) {
                md.Artists.emplace_back(artist["name"]);
            }
            md.AlbumName = json["album"]["name"];
            md.AlbumType = json["album"]["album_type"];
            md.ReleaseDate = json["album"]["release_date"];
            md.CoverUrl = json["album"]["images"][0]["url"];
            md.TotalTracks = json["album"]["total_tracks"];
            md.TrackNum = json["track_number"];
            md.DiscNum = json["disc_number"];

            if (json["external_ids"].contains("isrc")) {
                md.ISRC = json["external_ids"]["isrc"];
            } else {
                md.ISRC = "unknown";
            }
        } else if (json["type"] == "episode") {
            md.Type = TrackType::PodcastEpisode;
            md.TrackName = json["name"];
            md.Artists.emplace_back(json["show"]["publisher"]);
            md.AlbumName = json["show"]["name"];
            md.ReleaseDate = json["release_date"];
            md.CoverUrl = json["images"][0]["url"];
            md.TotalTracks = json["show"]["total_episodes"];
            md.TrackNum = 0;
            md.Description = json["description"];
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

        auto FillArg = [&](const std::string& name, const std::string& value) {
            auto repl = Utils::RemoveInvalidPathChars(value);
            Utils::StrReplace(fmt, name, repl);
        };
        FillArg("{artist_name}", metadata.Artists[0]);
        FillArg("{album_name}", metadata.AlbumName);
        FillArg("{track_name}", metadata.TrackName);
        FillArg("{track_num}", std::to_string(metadata.TrackNum));
        FillArg("{release_year}", std::to_string(metadata.GetReleaseYear()));
        
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