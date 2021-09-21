#include "pch.h"
#include "StateManager.h"
#include "Utils/Log.h"
#include "Utils/Http.h"

namespace fs = std::filesystem;
using namespace nlohmann;

struct PlaybackInfo
{
    std::string TrackId;
    bool WasSeeked;
};
struct OggStream
{
    fs::path FileName;
    std::ofstream FileStream;
    int NumPages;
    //Freq of playbacks during this stream.
    //Used to determine which track this stream is comming from.
    std::unordered_map<std::shared_ptr<PlaybackInfo>, int> SrcPlaybacks;
};

struct StateManagerImpl : public StateManager
{
    fs::path _dataDir;
    std::string _accessToken;

    std::unordered_map<std::string, std::shared_ptr<PlaybackInfo>> _playbacks;
    std::shared_ptr<PlaybackInfo> _currPlayback;

    std::unordered_map<uintptr_t, std::shared_ptr<OggStream>> _oggs;
    int _nextStreamId;

    json _config;

    StateManagerImpl(const fs::path& dataDir) : 
        _dataDir(dataDir),
        _nextStreamId(0)
    {
        std::ifstream configFile(dataDir / "config.json");
        if (configFile.good()) {
            _config = json::parse(configFile, nullptr, true, true);
        } else {
            _config["track_path_fmt"] = "%userprofile%/Music/Soggfy/{artist_name}/{album_name}/{track_num}. {track_name}.ogg";
            _config["cover_path_fmt"] = "%userprofile%/Music/Soggfy/{artist_name}/{album_name}/cover.jpg";
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
    void OnTrackOpened(const std::string& playbackId)
    {
        auto itr = _playbacks.find(playbackId);
        if (itr == _playbacks.end()) {
            LogWarn("Track opened for unknown playback, ignoring.");
            _currPlayback = nullptr;
            return;
        }
        _currPlayback = itr->second;
        LogInfo("New track detected: {}", _currPlayback->TrackId);
    }
    void OnTrackClosed(const std::string& playbackId)
    {
        LogDebug("TrackClosed: playId={}", playbackId);

        _playbacks.erase(playbackId);
        _currPlayback = nullptr;
        //TODO: dispose of ogg streams
    }
    void OnTrackSeeked(const std::string& playbackId)
    {
        auto itr = _playbacks.find(playbackId);
        if (itr != _playbacks.end()) {
            LogInfo("Current track was seeked, download will be cancelled.");
            itr->second->WasSeeked = true;
        }
    }

    std::shared_ptr<OggStream> GetOggStream(uintptr_t syncId)
    {
        auto itr = _oggs.find(syncId);
        if (itr == _oggs.end()) {
            auto stream = std::make_shared<OggStream>();

            stream->FileName = _dataDir / "temp" / ("stream_" + std::to_string(_nextStreamId++) + ".ogg");
            fs::create_directories(stream->FileName.parent_path());

            stream->FileStream.open(stream->FileName, std::ios::out | std::ios::binary);

            LogDebug("Detected new ogg stream, dumping to {}", stream->FileName.string());

            _oggs[syncId] = stream;
            return stream;
        }
        return itr->second;
    }

    std::shared_ptr<PlaybackInfo> FindSourcePlayback(std::shared_ptr<OggStream> stream)
    {
        std::pair<std::shared_ptr<PlaybackInfo>, int> best = {};
        for (auto& entry : stream->SrcPlaybacks) {
            if (entry.second > best.second) {
                best = entry;
            }
        }
        return best.first;
    }

    json FetchTrackMetadata(const std::string& trackId)
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
        return json::parse(rawJson.begin(), rawJson.end());
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

    //Call WINAPI ExpandEnvironmentStrings(), assuming str is UTF8.
    std::string ExpandEnvVars(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;
        std::wstring srcW = utfConv.from_bytes(str);

        DWORD len = ExpandEnvironmentStrings(srcW.c_str(), NULL, 0);

        std::wstring dstW(len, '\0');
        ExpandEnvironmentStrings(srcW.c_str(), dstW.data(), len);

        return utfConv.to_bytes(dstW);
    }
    fs::path RenderTrackPath(const std::string& fmtKey, const json& metadata)
    {
        std::string fmt = _config[fmtKey].get<std::string>();

        auto replace = [&](const std::string& needle, const std::string& replacement) {
            //https://stackoverflow.com/a/4643526
            size_t index = 0;
            while (true) {
                index = fmt.find(needle, index);
                if (index == std::string::npos) break;

                fmt.replace(index, needle.length(), replacement);
                index += replacement.length();
            }
        };
        replace("{artist_name}", metadata["artists"][0]["name"]);
        replace("{album_name}", metadata["album"]["name"]);
        replace("{track_name}", metadata["name"]);
        replace("{track_num}", std::to_string(metadata["track_number"].get<int>()));

        return ExpandEnvVars(fmt);
    }
    void WriteTags(const fs::path& path, const fs::path& coverPath, const json& meta)
    {
        std::ifstream coverFile(coverPath, std::ios::binary);
        auto coverData = std::vector<char>(std::istreambuf_iterator<char>(coverFile), std::istreambuf_iterator<char>());

        std::string artists;
        for (auto& artist : meta["artists"]) {
            if (artists.size() > 0) artists += ", ";
            artists += artist["name"];
        }
        auto releaseDate = meta["album"]["release_date"].get<std::string>();
        int year = std::stoi(releaseDate.substr(0, releaseDate.find('-')));

        TagLib::Ogg::Vorbis::File ogg(path.string().c_str());
        auto* tag = ogg.tag();
        
        auto coverArt = new TagLib::FLAC::Picture();
        coverArt->setType(TagLib::FLAC::Picture::Type::FrontCover);
        coverArt->setMimeType("image/jpeg");
        coverArt->setDescription("Front Cover");
        coverArt->setData(TagLib::ByteVector(coverData.data(), (uint32_t)coverData.size()));

        tag->addPicture(coverArt);
        tag->setTitle(meta["name"].get<std::string>());
        tag->setArtist(artists);
        tag->setAlbum(meta["album"]["name"].get<std::string>());
        tag->setTrack(meta["track_number"].get<int>());
        tag->setYear(year);

        tag->addField("DATE", releaseDate);
        tag->addField("DISCNUMBER", std::to_string(meta["disc_number"].get<int>()));
        tag->addField("ISRC", meta["external_ids"]["isrc"].get<std::string>());
        tag->addField("RELEASETYPE", meta["album"]["album_type"].get<std::string>());
        tag->addField("TOTALTRACKS", std::to_string(meta["album"]["total_tracks"].get<int>()));

        ogg.save();
    }
    bool TagAndMoveToOutput(std::shared_ptr<OggStream> stream)
    {
        //There's always a short stream just before the actual track
        if (stream->NumPages < 8) return false;

        auto playback = FindSourcePlayback(stream);
        if (playback == nullptr || playback->WasSeeked) return false;

        if (_accessToken.empty()) {
            LogWarn("Access token not available, discarding current track...");
            return false;
        }
        auto& trackId = playback->TrackId;
        auto meta = FetchTrackMetadata(trackId);

        LogInfo("Saving track {} -> {} - {}", trackId, meta["artists"][0]["name"].get<std::string>(), meta["name"].get<std::string>());
        LogDebug("Meta: {}", meta.dump());

        auto trackPath = RenderTrackPath("track_path_fmt", meta);
        auto coverPath = RenderTrackPath("cover_path_fmt", meta);

        auto tmpCoverPath = _dataDir / "temp" / (meta["album"]["id"].get<std::string>() + "_cover.jpg");

        if (!fs::exists(tmpCoverPath)) {
            DownloadFile(tmpCoverPath, meta["album"]["images"][0]["url"]);
        }
        if (!coverPath.empty() && !fs::exists(coverPath)) {
            fs::create_directories(coverPath.parent_path());
            fs::copy_file(tmpCoverPath, coverPath);
        }
        WriteTags(stream->FileName, tmpCoverPath, meta);

        fs::create_directories(trackPath.parent_path());
        fs::rename(stream->FileName, trackPath);

        return true;
    }
    void FinalizeOggStream(std::shared_ptr<OggStream> stream)
    {
        try {
            if (!TagAndMoveToOutput(stream)) {
                LogDebug("Deleting bad stream: {}", stream->FileName.string());
                std::filesystem::remove(stream->FileName);
            }
        } catch (std::exception& ex) {
            LogError("Failed to finalize stream: {}", ex.what());
        }
    }

    void CloseOggStream(uintptr_t syncId, std::shared_ptr<OggStream> stream)
    {
        LogDebug("Closing {}", stream->FileName.string());

        stream->FileStream.close();

        //run this on a new thread bc download may block the decoder thread
        std::thread t(&StateManagerImpl::FinalizeOggStream, this, stream);
        t.detach();

        _oggs.erase(syncId);
    }

    void ReceiveOggPage(uintptr_t syncId, ogg_page* page)
    {
        auto state = GetOggStream(syncId);

        state->FileStream.write((char*)page->header, page->header_len);
        state->FileStream.write((char*)page->body, page->body_len);
        state->NumPages++;
        if (_currPlayback != nullptr) {
            state->SrcPlaybacks[_currPlayback]++;
        }
        if (page->header[5] & 0x04) { //ogg_page_eos
            CloseOggStream(syncId, state);
        }
    }

    void UpdateAccToken(const std::string& token)
    {
        LogDebug("Update access token: {}", token);
        _accessToken = token;
    }
};

std::unique_ptr<StateManager> StateManager::New(const fs::path& dataDir)
{
    return std::make_unique<StateManagerImpl>(dataDir);
}