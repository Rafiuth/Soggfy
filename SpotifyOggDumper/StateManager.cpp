#include "pch.h"
#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Http.h"

namespace fs = std::filesystem;
using namespace nlohmann;

struct PlaybackInfo;
struct OggStream;

struct PlaybackInfo
{
    std::string TrackId;
    bool WasSeeked;
    int StartPos;
    bool Closed;
    bool PlayedToEnd; //whether closeReason == "trackdone"
    
    //Map of <Stream, Freq> that were alive during this playback.
    //Used to determine stream source playbacks.
    std::unordered_map<std::shared_ptr<OggStream>, int> LiveStreams;
};
struct OggStream
{
    fs::path FileName;
    std::ofstream FileStream;
    int NumPages;
    bool EOS;
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
    int TrackNum;
    int DiscNum;
    int TotalTracks;

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
    std::shared_ptr<PlaybackInfo> _currPlayback;

    std::unordered_map<uintptr_t, std::shared_ptr<OggStream>> _oggs;
    int _nextStreamId;

    std::string _ffmpegPath;
    json _config;

    StateManagerImpl(const fs::path& dataDir) : 
        _dataDir(dataDir),
        _nextStreamId(0)
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

        if (_config.contains("convert_args") && (_ffmpegPath = FindFFmpegPath().string()).empty()) {
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
        _playbacks[playbackId] = info;
    }
    void OnTrackOpened(const std::string& playbackId, int positionMs)
    {
        auto itr = _playbacks.find(playbackId);
        if (itr == _playbacks.end()) {
            LogWarn("Track opened for unknown playback, ignoring.");
            _currPlayback = nullptr;
            return;
        }
        _currPlayback = itr->second;
        _currPlayback->StartPos = positionMs;
        LogInfo("New track detected: {}", _currPlayback->TrackId);
    }
    void OnTrackClosed(const std::string& playbackId, const std::string& reason)
    {
        LogDebug("TrackClosed: playId={} reason={}", playbackId, reason);
        
        auto itr = _playbacks.find(playbackId);
        if (itr != _playbacks.end()) {
            auto playback = itr->second;

            playback->Closed = true;
            playback->PlayedToEnd = reason == "trackdone";

            if (playback == _currPlayback) {
                _currPlayback = nullptr;
            }
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
    void ReceiveOggPage(uintptr_t syncId, ogg_page* page)
    {
        auto state = GetOggStream(syncId);

        if (!state->EOS) {
            state->FileStream.write((char*)page->header, page->header_len);
            state->FileStream.write((char*)page->body, page->body_len);
            state->NumPages++;

            if (_currPlayback != nullptr) {
                _currPlayback->LiveStreams[state]++;
            }
            if (page->header[5] & 0x04) { //ogg_page_eos
                state->EOS = true;
                state->FileStream.close();
                _oggs.erase(syncId);

                FlushClosedTracks();
            }
        }
    }
    void UpdateAccToken(const std::string& token)
    {
        LogDebug("Update access token: {}", token);
        _accessToken = token;
    }
    void Shutdown()
    {
        for (auto& [id, stream] : _oggs) {
            stream->FileStream.close();
        }
    }

    std::shared_ptr<OggStream> GetOggStream(uintptr_t syncId)
    {
        auto itr = _oggs.find(syncId);
        if (itr != _oggs.end()) {
            if (itr->second->EOS) {
                LogWarn("Detected a possible collision with ogg stream ids, last few tracks could become corrupted.");
            }
            return itr->second;
        }
        auto stream = std::make_shared<OggStream>();

        stream->FileName = _dataDir / "temp" / ("stream_" + std::to_string(_nextStreamId++) + ".ogg");
        fs::create_directories(stream->FileName.parent_path());

        stream->FileStream.open(stream->FileName, std::ios::out | std::ios::binary);

        LogDebug("Detected new ogg stream, dumping to {}", stream->FileName.string());

        _oggs[syncId] = stream;
        return stream;
    }
    
    void FlushClosedTracks()
    {
        if (_accessToken.empty()) {
            LogDebug("FlushTracks: access token not available yet, deferring...");
            return;
        }
        //Iterate and remove finished playbacks
        auto numFlushed = std::erase_if(_playbacks, [&](const auto& elem) {
            auto playback = elem.second;
            if (!playback->Closed) return false;

            auto stream = FindSourceStream(playback);
            if (stream == nullptr) return false; //defer until a the stream is closed
            
            if (playback->StartPos == 0 && playback->PlayedToEnd && !playback->WasSeeked) {
                std::thread t(&StateManagerImpl::TagAndMoveToOutput, this, playback, stream);
                t.detach();
            } else {
                LogInfo("Discarding track {} because it was not fully played.", playback->TrackId);
            }
            return true;
        });
        LogDebug("FlushTracks: flushed {} tracks. Alive: {} playbacks, {} streams.", numFlushed, _playbacks.size(), _oggs.size());
    }
    std::shared_ptr<OggStream> FindSourceStream(std::shared_ptr<PlaybackInfo> playback)
    {
        std::shared_ptr<OggStream> bestStream;
        int bestFreq = 8; //ignore short streams

        for (auto& [stream, freq] : playback->LiveStreams) {
            if (freq > bestFreq) {
                bestStream = stream;
                bestFreq = freq;
            }
        }
        return bestStream;
    }

    void TagAndMoveToOutput(std::shared_ptr<PlaybackInfo> playback, std::shared_ptr<OggStream> stream)
    {
        auto& trackId = playback->TrackId;

        try {
            auto meta = FetchTrackMetadata(trackId);

            LogInfo("Saving track {}", trackId);
            LogInfo("  title: {} - {}", meta.Artists[0], meta.TrackName);
            LogDebug("  stream: {}", stream->FileName.filename().string());
            LogDebug("  meta: {}", meta.RawData.dump());

            auto pathKeys = meta.Type == TrackType::Music
                ? std::make_tuple("track_path_fmt", "cover_path_fmt")
                : std::make_tuple("podcast_path_fmt", "podcast_cover_path_fmt");

            auto trackPath = RenderTrackPath(std::get<0>(pathKeys), meta);
            auto coverPath = RenderTrackPath(std::get<1>(pathKeys), meta);

            auto tmpCoverPath = _dataDir / "temp" / (SanitizeFilename(meta.CoverUrl) + ".jpg");

            if (!fs::exists(tmpCoverPath)) {
                DownloadFile(tmpCoverPath, meta.CoverUrl);
            }
            if (!coverPath.empty() && !fs::exists(coverPath)) {
                fs::create_directories(coverPath.parent_path());
                fs::copy_file(tmpCoverPath, coverPath);
            }
            WriteTags(stream->FileName, tmpCoverPath, meta);
            CreateFinalTrack(stream->FileName, trackPath, meta);
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

            std::string args = std::format("-i \"{}\" {} \"{}\"", path.string(), convArgs.get<std::string>(), outPath.string());

            LogInfo("Converting {} - {}...", meta.Artists[0], meta.TrackName);
            LogDebug("  args: {}", args);

            DWORD exitCode = StartProcess(_ffmpegPath, args, true);
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

        TagLib::Ogg::Vorbis::File ogg(path.string().c_str());
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
    uint32_t StartProcess(const std::string& filename, const std::string& args, bool waitForExit)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;
        std::wstring filenameW = utfConv.from_bytes(filename);
        std::wstring argsW = utfConv.from_bytes("\"" + filename + "\" " + args);

        DWORD exitCode = 0;

        PROCESS_INFORMATION processInfo = {};
        STARTUPINFO startInfo = {};
        startInfo.cb = sizeof(STARTUPINFO);
        startInfo.dwFlags = STARTF_USESHOWWINDOW;
        startInfo.wShowWindow = SW_HIDE;

        DWORD flags = CREATE_NEW_CONSOLE;

        if (!CreateProcess(filenameW.c_str(), argsW.data(), NULL, NULL, FALSE, flags, NULL, _dataDir.wstring(), &startInfo, &processInfo)) {
            throw std::runtime_error("Failed to create process " + filename + " (error " + std::to_string(GetLastError()) + ")");
        }
        if (waitForExit) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
        }
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return exitCode;
    }
    
    fs::path RenderTrackPath(const std::string& fmtKey, const TrackMetadata& metadata)
    {
        std::string fmt = _config[fmtKey].get<std::string>();

        auto FillArg = [&](const std::string& needle, const std::string& replacement) {
            auto repl = SanitizeFilename(replacement);
            //https://stackoverflow.com/a/4643526
            size_t index = 0;
            while (true) {
                index = fmt.find(needle, index);
                if (index == std::string::npos) break;

                fmt.replace(index, needle.length(), repl);
                index += repl.length();
            }
        };
        FillArg("{artist_name}", metadata.Artists[0]);
        FillArg("{album_name}", metadata.AlbumName);
        FillArg("{track_name}", metadata.TrackName);
        FillArg("{track_num}", std::to_string(metadata.TrackNum));
        FillArg("{release_year}", std::to_string(metadata.GetReleaseYear()));
        
        return fs::u8path(ExpandEnvVars(fmt));
    }
    //Call WINAPI ExpandEnvironmentStrings(), assuming str is UTF8.
    std::string ExpandEnvVars(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;
        std::wstring srcW = utfConv.from_bytes(str);

        DWORD len = ExpandEnvironmentStrings(srcW.c_str(), NULL, 0);

        std::wstring dstW(len, '\0');
        ExpandEnvironmentStrings(srcW.c_str(), dstW.data(), len);
        dstW.resize(len - 1); //don't count null terminator in .size()

        return utfConv.to_bytes(dstW);
    }
    std::string SanitizeFilename(const std::string& src)
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
};

std::unique_ptr<StateManager> StateManager::New(const fs::path& dataDir)
{
    return std::make_unique<StateManagerImpl>(dataDir);
}