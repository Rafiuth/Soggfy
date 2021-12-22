import { PlayerState } from "./spotify-apis";

type PathVar = {
    name: string;
    desc: string;
    pattern: string;
    getValue: (meta: any, playback: PlayerState) => string;
};

export type PathTemplateVars = { [key: string]: string };

export class PathTemplate
{
    static readonly Vars: PathVar[] = [
        {
            name: "track_name",
            desc: "Track name / Episode name",
            pattern: `.+`,
            getValue: m => m.title
        },
        {
            name: "artist_name",
            desc: "Artist name / Publisher name",
            pattern: `.+`,
            getValue: m => m.album_artist
        },
        {
            name: "album_name",
            desc: "Album name / Podcast name",
            pattern: `.+`,
            getValue: m => m.album
        },
        {
            name: "track_num",
            desc: "Album track number",
            pattern: `\\d+`,
            getValue: m => m.track
        },
        {
            name: "release_year",
            desc: "Release year",
            pattern: `\\d+`,
            getValue: m => m.date.split('-')[0]
        },
        {
            name: "release_date",
            desc: "Release date in YYYY-MM-DD format",
            pattern: `\\d+-\\d+-\\d+`,
            getValue: m => m.date
        },
        {
            name: "multi_disc_path",
            desc: "'/CD {disc number}' if the album has multiple discs, or empty.",
            pattern: `(\\/CD \\d+)?`,
            getValue: m => m.totaldiscs > 1 ? `/CD ${m.disc}` : ""
        },
        {
            name: "multi_disc_paren",
            desc: "' (CD {disc number})' if the album has multiple discs, or empty.",
            pattern: `( \\(CD \\d+\\))?`,
            getValue: m => m.totaldiscs > 1 ? ` (CD ${m.disc})` : ""
        },
        {
            name: "playlist_name",
            desc: "Playlist name or 'unknown' if the track playback didn't originate from a playlist.",
            pattern: `.+`,
            getValue: (m, s) => {
                return s.context.uri.startsWith("spotify:playlist:")
                    ? s.context.metadata.context_description
                    : "unknown";
            }
        },
        {
            name: "context_name",
            desc: "Context name or 'unknown' - Similar to {playlist_name}, but includes albums.",
            pattern: `.+`,
            getValue: (m, s) => {
                return s.context.metadata.context_description ?? "unknown";
            }
        }
    ];

    static getVarsFromMetadata(meta: any, playback: PlayerState)
    {
        let vals: PathTemplateVars = {};
        for (let pv of PathTemplate.Vars) {
            vals[pv.name] = pv.getValue(meta, playback);
        }
        return vals;
    }

    static render(template: string, vars: PathTemplateVars)
    {
        return template.replace(/{(.+)}/g, (g0, g1) => {
            return vars[g1] ?? g0;
        });
    }
}