import Utils from "./utils";
import { Player, PlayerState, TrackInfo } from "./spotify-apis";
import Resources from "./resources";
import config from "./config";

class PlayerStateTracker
{
    private _playbacks: Map<string, PlayerState>;

    constructor()
    {
        this._playbacks = new Map();

        Utils.createHook(Player._events._emitter.__proto__, "createEvent", (stage, args, ret) => {
            let eventType = args[0];
            let data = args[1];
            if (stage === "pre" && eventType === "update" && data?.playbackId) {
                if (!this._playbacks.has(data.playbackId)) {
                    this._playbacks.set(data.playbackId, data);
                }
            }
        });
    }

    /** Returns limited track metadata for a given playbackId, or null. */
    getTrackInfo(playbackId: string)
    {
        return this._playbacks.get(playbackId)?.item;
    }
    remove(playbackId: string)
    {
        return this._playbacks.delete(playbackId);
    }
    /** Returns complete metadata for a given playback id. */
    async getMetadata(playbackId: string)
    {
        let playback = this._playbacks.get(playbackId);
        let track = playback.item;
        let type = this.getTrackType(track.uri);

        let meta: any = type === "track"
            ? await this.getTrackMetaProps(track)
            : await this.getPodcastMetaProps(track);
        
        let data = {
            type: type,
            playbackId: playback.playbackId,
            trackUri: track.uri,
            metadata: meta,
            pathVars: this.getPathVariables(meta),
            lyrics: "",
            lyricsExt: "",
            coverArtId: track.metadata.image_xlarge_url.replaceAll(":", "_")
        };
        let coverData = await Resources.getImageData(track.metadata.image_xlarge_url);

        let lyrics = config.downloadLyrics ? await this.getLyrics(track) : null;
        if (lyrics) {
            data.lyrics = this.convertLyricsToLRC(lyrics);
            data.lyricsExt = lyrics.isSynced ? "lrc" : "txt";
            data.metadata.lyrics = lyrics.lines.map(v => v.text).join('\n');
        }
        return { info: data, coverData: coverData };
    }
    private async getTrackMetaProps(track: TrackInfo)
    {
        let meta = track.metadata;
        let extraMeta = await Resources.getTrackMetadataWG(track.uri);

        //https://community.mp3tag.de/t/a-few-questions-about-the-disc-number-disc-total-columns/18698/8
        //https://help.mp3tag.de/main_tags.html
        //TODO: mp3 uses "track: x/n", totaltracks and totaldiscs is vorbis only
        let { year, month, day } = extraMeta.album.date;
        let date = [year, month, day];
        //Truncate date to available precision (year only sample: https://open.spotify.com/track/4VxaUj96W2jw9UOtKHu51p)
        if (!day) date.pop();
        if (!month) date.pop();
        
        return {
            title:          meta.title,
            album_artist:   meta.artist_name,
            album:          meta.album_title,
            artist:         extraMeta.artist.map(v => v.name).join("/"),
            track:          meta.album_track_number,
            totaltracks:    meta.album_track_count,
            disc:           meta.album_disc_number,
            totaldiscs:     meta.album_disc_count,
            date:           date.map(x => Utils.padInt(x, 2)).join('-'), //YYYY-MM-DD,
            publisher:      extraMeta.album.label,
            language:       extraMeta.language_of_performance?.[0],
            isrc:           extraMeta.external_id.find(v => v.type === "isrc")?.id,
            comment:        Resources.getOpenTrackURL(track.uri),
            ITUNESADVISORY: meta.is_explicit ? "1" : undefined
        };
    }
    private async getPodcastMetaProps(track: TrackInfo)
    {
        let meta = await Resources.getEpisodeMetadata(track.uri);

        return {
            title:          meta.name,
            album:          meta.show.name,
            album_artist:   meta.show.publisher,
            description:    meta.description,
            podcastdesc:    meta.show.description,
            podcasturl:     meta.external_urls.spotify,
            publisher:      meta.show.publisher,
            date:           meta.release_date,
            language:       meta.language,
            comment:        Resources.getOpenTrackURL(track.uri),
            podcast:        "1",
            ITUNESADVISORY: meta.explicit ? "1" : undefined
        };
    }
    private getPathVariables(meta: any)
    {
        return {
            track_name:     meta.title,
            artist_name:    meta.album_artist,
            album_name:     meta.album,
            track_num:      meta.track,
            release_year:   meta.date.split('-')[0],
            multi_disc_path: meta.totaldiscs > 1 ? `/CD ${meta.disc}` : "",
            multi_disc_paren: meta.totaldiscs > 1 ? ` (CD ${meta.disc})` : ""
        };
    }
    private getTrackType(uri: string): "track" | "podcast" | "unknown"
    {
        if (uri.startsWith("spotify:track:")) {
            return "track";
        }
        if (uri.startsWith("spotify:episode:")) {
            return "podcast";
        }
        return "unknown";
    }
    private async getLyrics(track: TrackInfo)
    {
        if (track.metadata.has_lyrics !== "true") {
            return null;
        }
        let resp = await Resources.getColorAndLyricsWG(track.uri, track.metadata.image_url);
        let lyrics = resp.lyrics;

        return {
            lang: lyrics.language,
            isSynced: ["LINE_SYNCED", "SYLLABLE_SYNCED"].includes(lyrics.syncType),
            lines: lyrics.lines.map(ln => ({
                time: parseInt(ln.startTimeMs),
                text: ln.words
            }))
        };
    }
    private convertLyricsToLRC(data)
    {
        //https://en.wikipedia.org/wiki/LRC_(file_format)#Simple_format
        //https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/lrcdec.c#L88
        //(number of digits doesn't seem to matter)
        let text = "";
        for (let line of data.lines) {
            if (data.isSynced) {
                let time = line.time;
                let mm = Utils.padInt(time / 1000 / 60, 2);
                let ss = Utils.padInt(time / 1000 % 60, 2);
                let cs = Utils.padInt(time % 1000 / 10, 2);
                text += `[${mm}:${ss}.${cs}]`;
            }
            text += line.text + '\n';
        }
        return text;
    }
}

export default PlayerStateTracker;