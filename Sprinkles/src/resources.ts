import { WebAPI } from "./spotify-apis";

class Resources
{
    static async getTrackMetadataWG(uri: string)
    {
        let trackId = this.idToHex(this.getUriId(uri, "track"));
        let res = await WebAPI.spotifyTransport.build()
            .withHost("https://spclient.wg.spotify.com/metadata/4")
            .withPath(`/track/${trackId}`)
            .withEndpointIdentifier("/track/{trackId}")
            .send();
        return res.body as TrackMetadataWG;
    }
    static async getColorAndLyricsWG(trackUri: string, coverUri: string)
    {
        let res = await WebAPI.spotifyTransport.build()
            .withHost("https://spclient.wg.spotify.com/color-lyrics/v2")
            .withPath(`/track/${this.getUriId(trackUri)}/image/${encodeURIComponent(coverUri)}`)
            .withQueryParameters({
                format: "json",
                vocalRemoval: true
            }).withEndpointIdentifier("/track/{trackId}")
            .send();
        return res.body as ColorAndLyricsWG;
    }
    static async getEpisodeMetadata(uri: string)
    {
        return await WebAPI.getEpisode(this.getUriId(uri, "episode"));
    }
    static async getAlbumTracks(uri: string, offset: number, limit: number)
    {
        let id = this.getUriId(uri, "album");
        let params = {
            offset: offset,
            limit: limit
        };
        let ep = WebAPI.endpoints.Album;
        let resp = await ep.getAlbumTracks(WebAPI.spotifyTransport, id, params);
        return resp.body;
    }

    static async getImageData(uri: string)
    {
        //Spotify actually does some blackmagic and sets this id directly to <img> src
        //There's no way to get the original data out of it without reencoding.
        //this EP is public so it's probably ok to do another request
        let url = "https://i.scdn.co/image/" + this.getUriId(uri, "image");
        let resp = await fetch(url);
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