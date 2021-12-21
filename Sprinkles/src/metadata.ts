type PathVar = {
    name: string;
    desc: string;
    pattern: string;
    getValue: (meta) => string;
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
            desc: "`/CD {disc number}` if the album has multiple discs, or empty.",
            pattern: `(\\/CD \\d+)?`,
            getValue: m => m.totaldiscs > 1 ? `/CD ${m.disc}` : ""
        },
        {
            name: "multi_disc_paren",
            desc: "` (CD {disc number})` if the album has multiple discs, or empty.",
            pattern: `( \\(CD \\d+\\))?`,
            getValue: m => m.totaldiscs > 1 ? ` (CD ${m.disc})` : ""
        }
    ];

    static getVarsFromMetadata(meta)
    {
        let vals: PathTemplateVars = {};
        for (let pv of PathTemplate.Vars) {
            vals[pv.name] = pv.getValue(meta);
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