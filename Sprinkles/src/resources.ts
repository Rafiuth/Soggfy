import { Platform, SpotifyUtils, WebAPI } from "./spotify-apis";

class Resources
{
    static getTrackMetadataWG(uri: string): Promise<TrackMetadataWG>
    {
        let trackId = this.idToHex(this.getUriId(uri, "track"));
        return this.fetchAuthed({
            url: `https://spclient.wg.spotify.com/metadata/4/track/${trackId}`
        });
    }
    static getColorAndLyricsWG(trackUri: string, coverUri: string): Promise<ColorAndLyricsWG>
    {
        return this.fetchAuthed({
            url: `https://spclient.wg.spotify.com/color-lyrics/v2/track/${this.getUriId(trackUri)}/image/${encodeURIComponent(coverUri)}`,
            params: {
                format: "json",
                vocalRemoval: true
            }
        });
    }
    static async getEpisodeMetadata(uri: string)
    {
        return await WebAPI.getEpisode(this.getUriId(uri, "episode"));
    }

    static async getPlaylistTracks(uri: string, sorted = true)
    {
        let api = Platform.getPlaylistAPI();
        let metadata = await api.getMetadata(uri);

        let sortState = sorted ? SpotifyUtils.getPlaylistSortState(uri) : undefined;
        let contents = await api.getContents(uri, { sort: sortState });

        return { ...metadata, tracks: contents };
    }
    static async getAlbumTracks(uri: string)
    {
        let id = this.getUriId(uri, "album");
        let result = await this.fetchAuthed({ url: `https://api.spotify.com/v1/albums/${id}` });
        
        let nextPageUrl = result.tracks.next;
        while (nextPageUrl != null) {
            let page = await this.fetchAuthed({ url: nextPageUrl });
            result.tracks.items.push(page.items);
            nextPageUrl = page.next;
        }
        return result;
    }
    
    /** Returns a object with metadata and variables for all tracks in the specified album or playlist. */
    static async getTracks(uri: string)
    {
        let type = this.getUriType(uri);

        if (type === "playlist") {
            let data = await this.getPlaylistTracks(uri, true);
            
            return {
                name: data.name,
                type: "playlist",
                tracks: data.tracks.items.map(track => ({
                    uri: track.uri,
                    durationMs: track.duration.milliseconds,
                    vars: {
                        track_name: track.name,
                        artist_name: track.artists[0].name,
                        all_artist_names: track.artists.map(v => v.name).join(", "),
                        album_name: track.album.name,
                        track_num: track.trackNumber,
                        playlist_name: data.name,
                        context_name: data.name
                    }
                }))
            };
        }
        if (type === "album") {
            let data = await this.getAlbumTracks(uri);
            
            return {
                name: data.name,
                type: "album",
                tracks: data.tracks.items.map(track => ({
                    uri: track.uri,
                    durationMs: track.duration_ms,
                    vars: {
                        track_name: track.name,
                        artist_name: track.artists[0].name,
                        all_artist_names: track.artists.map(v => v.name).join(", "),
                        album_name: data.name,
                        track_num: track.track_number,
                        playlist_name: "unknown",
                        context_name: data.name
                    }
                }))
            };
        }
        throw Error("Unknown collection type: " + uri);
    }

    /**
     * Fetches a JSON object from an authenticated Spotify endpoint.
     */
    static async fetchAuthed(init: SpRequestInit)
    {
        const spt = WebAPI.spotifyTransport;
        
        let req: RequestInit = {
            method: init.method,
            headers: {
                "Authorization": "Bearer " + spt.accessToken,
                "Accept": "application/json",
                "Accept-Language": spt.locale,
                ...Object.fromEntries(spt.globalRequestHeaders)
            }
        };
        if (init.body) {
            req.body = JSON.stringify(init.body);
            req.headers["Content-Type"] = "application/json;charset=UTF-8";
        }

        let url = new URL(init.url);
        for (let [k, v] of Object.entries(init.params ?? {})) {
            url.searchParams.append(k, v);
        }
        url.searchParams.append("market", spt.market);
        
        let resp = await fetch(url.toString(), req);
        if (!resp.ok) {
            throw Error(`Failed to fetch ${url.toString()}: ${resp.status} ${resp.statusText}`);
        }
        return await resp.json();
    }

    static getImageData(uri: string)
    {
        //Spotify actually does some blackmagic and sets this id directly to <img> src
        //There's no way to get the original data out of it without reencoding.
        //this EP is public so it's probably ok to do another request
        return this.fetchBytes("https://i.scdn.co/image/" + this.getUriId(uri, "image"));
    }
    static async fetchBytes(url: string)
    {
        let resp = await fetch(url);
        if (!resp.ok) {
            throw Error(`fetch("${url.toString()}") failed: ${resp.statusText}`);
        }
        return await resp.arrayBuffer();
    }

    static getOpenTrackURL(uri: string)
    {
        let parts = uri.split(':');
        return `https://open.spotify.com/${parts[1]}/${parts[2]}`;
    }
    static getUriId(uri: string, expType: string = null)
    {
        let parts = uri.split(':');
        if (expType && parts[1] !== expType) {
            throw Error(`Expected URI of type '${expType}', got '${uri}'`);
        }
        return parts[2];
    }
    static getUriType(uri: string)
    {
        return uri.split(':')[1];
    }

    /** Converts a hex string into a base62 spotify id */
    static hexToId(str: string): string
    {
        const alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        let val = BigInt("0x" + str);
        let digits = [];
        while (digits.length < 22) {
            let digit = Number(val % 62n);
            val /= 62n;
            digits.push(alphabet.charAt(digit));
        }
        return digits.reverse().join('');
    }
    /** Converts a base62 spotify id to a hex string */
    static idToHex(str: string): string
    {
        const alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        let val = 0n;
        for (let i = 0; i < str.length; i++) {
            let digit = alphabet.indexOf(str.charAt(i));
            val = (val * 62n) + BigInt(digit);
        }
        return val.toString(16).padStart(32, '0');
    }
}
interface SpRequestInit
{
    url: string;
    /** Additional url query parameters */
    params?: Record<string, any>;
    method?: string;
    body?: any;
}

interface TrackMetadataWG
{
    name: string;
    album: AlbumMetadataWG;
    artist: {
        name: string;
    }[]
    number: number;
    disc_number: number;
    duration: number,
    popularity: number,
    external_id: { type: string, id: string }[];
    has_lyrics: boolean;
    language_of_performance: string[];
    original_title: string;
}
interface AlbumMetadataWG
{
    name: string;
    label: string;
    date: {
        year: number;
        month?: number;
        day?: number;
    }
}
interface ColorAndLyricsWG
{
    colors: {
        background: number,
        text: number,
        highlightText: number
    },
    hasVocalRemoval: false,
    lyrics: {
        syncType: "UNSYNCED" | "LINE_SYNCED" | "SYLLABLE_SYNCED",
        lines: {
            startTimeMs: string,
            words: string,
            syllables: any[]
        }[]
        provider: string,
        providerLyricsId: number,
        providerDisplayName: string,
        syncLyricsUri: string,
        isDenseTypeface: boolean,
        alternatives: any[],
        language: string,
        isRtlLanguage: boolean,
        fullscreenAction: string
    }
}

export default Resources;
export {
    TrackMetadataWG,
    AlbumMetadataWG,
    ColorAndLyricsWG
};