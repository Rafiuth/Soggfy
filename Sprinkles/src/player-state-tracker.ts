import Utils from "./utils";
import { Player, PlayerState, TrackInfo, SpotifyUtils } from "./spotify-apis";
import Resources from "./resources";
import config from "./config";
import { PathTemplate, PathTemplateVars } from "./metadata";

export default class PlayerStateTracker
{
    private _playbacks: Map<string, PlayerState>;

    constructor(stateChanged?: (newState: PlayerState, oldState?: PlayerState) => void)
    {
        this._playbacks = new Map();

        Utils.createHook(Player._events._emitter.__proto__, "createEvent", (stage, args, ret) => {
            let eventType = args[0];
            let data = args[1];
            if (stage === "pre" && eventType === "update" && data?.playbackId) {
                if (stateChanged) {
                    let oldState = this._playbacks.get(data.playbackId);
                    stateChanged?.(data, oldState);
                }
                this._playbacks.set(data.playbackId, data);
            }
        });
        //Player stops sometimes when speed is too high (>= 30)
        Player._client.getError({}, err => {
            if (err.message === "playback_stuck" && err.data.playback_id === Player.getState().playbackId) {
                SpotifyUtils.resetCurrentTrack(false);
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
        let type = Resources.getUriType(track.uri);

        let meta: any = type === "track"
            ? await this.getTrackMetaProps(track)
            : await this.getPodcastMetaProps(track);
        let paths = this.getSavePaths(type, meta, playback);
        
        let data = {
            type: type,
            playbackId: playback.playbackId,
            trackUri: track.uri,
            metadata: meta,
            trackPath: paths.track,
            coverPath: paths.cover,
            lyrics: undefined,
            lyricsExt: undefined,
            coverTempPath: track.metadata.image_xlarge_url.replaceAll(":", "_")
        };
        let coverData = await Resources.getImageData(track.metadata.image_xlarge_url);

        if (config.saveLyrics || config.embedLyrics) {
            let lyrics = await this.getLyrics(track);

            if (lyrics && config.embedLyrics) {
                let text = lyrics.text;
                //workaround for the 32k command line limit
                //TODO: the proper solution is to use that FFMETADATA file with the lyrics
                if (text.length > 25000) {
                    text = text.substring(0, 25000);
                }
                data.metadata.lyrics = text;
            }
            if (lyrics && config.saveLyrics) {
                data.lyrics = lyrics.text;
                data.lyricsExt = lyrics.isSynced ? "lrc" : "txt";
            }
        }
        this.fixMetadata(data.metadata, config.outputFormat.ext || "ogg");
        return { info: data, coverData: coverData };
    }
    private getSavePaths(type: string, meta: any, playback: PlayerState)
    {
        let template = config.savePaths[type] as string;
        if (!template) throw Error("Unknown track type " + type);

        let path = config.savePaths.basePath;
        if (!/[\/\\]$/.test(path)) path += '/';

        let vars = PathTemplate.getVarsFromMetadata(meta, playback);

        let coverPath = "";
        if (config.saveCoverArt) {
            let parts = template.split(/[\/\\]/);
            let albumDirIdx = parts.findIndex(d => d.includes("{album_name}"));
            let hasAlbumDir = albumDirIdx >= 0 && albumDirIdx + 1 < parts.length;

            if (hasAlbumDir) {
                let coverTemplate = parts.slice(0, albumDirIdx + 1).join('/') + "/cover.jpg";
                coverPath = path + PathTemplate.render(coverTemplate, vars);
            }
        }
        return {
            track: path + PathTemplate.render(template, vars),
            cover: coverPath
        };
    }
    private fixMetadata(meta: any, format: string): any
    {
        //https://wiki.multimedia.cx/index.php?title=FFmpeg_Metadata
        //https://help.mp3tag.de/main_tags.html
        //https://github.com/yarrm80s/orpheusdl/blob/fed108e978083f92d96efcacdf09fe2dc8082bc3/orpheus/tagging.py

        if (["mp3", "mp4", "m4a"].includes(format)) {
            meta.track = `${meta.track}/${meta.totaltracks}`;
            meta.disc = `${meta.disc}/${meta.totaldiscs}`;
            delete meta.totaltracks;
            delete meta.totaldiscs;
        }
    }
    private async getTrackMetaProps(track: TrackInfo)
    {
        let meta = track.metadata;
        let extraMeta = await Resources.getTrackMetadataWG(track.uri);

        let { year, month, day } = extraMeta.album.date;
        let date = [year, month, day];
        //Truncate date to available precision
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
            explicit:       meta.is_explicit ? "1" : undefined
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
            explicit:       meta.explicit ? "1" : undefined
        };
    }
    private async getLyrics(track: TrackInfo)
    {
        if (track.metadata.has_lyrics !== "true") {
            return null;
        }
        let resp = await Resources.getColorAndLyricsWG(track.uri, track.metadata.image_url);
        let lyrics = resp.lyrics;

        let isSynced = ["LINE_SYNCED", "SYLLABLE_SYNCED"].includes(lyrics.syncType);

        let text = "";
        for (let line of lyrics.lines) {
            //skip empty lines
            if (!isSynced && /^(|♪)$/.test(line.words)) continue;

            if (isSynced) {
                //https://en.wikipedia.org/wiki/LRC_(file_format)#Simple_format
                //https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/lrcdec.c#L88
                //(number of digits doesn't seem to matter)
                let time = parseInt(line.startTimeMs);
                let mm = Utils.padInt(time / 1000 / 60, 2);
                let ss = Utils.padInt(time / 1000 % 60, 2);
                let cs = Utils.padInt(time % 1000 / 10, 2);
                text += `[${mm}:${ss}.${cs}]`;
            }
            text += line.words;
            text += '\n';
        }
        return { text: text, rawData: lyrics, isSynced: isSynced };
    }
}