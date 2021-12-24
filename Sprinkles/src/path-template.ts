import { PlayerState } from "./spotify-apis";

interface PathVar
{
    name: string;
    desc: string;
    pattern: string;
    getValue: (meta: any, playback: PlayerState) => string;
};

export type PathTemplateVars = Record<string, string>;

function createVarMap(vars: PathVar[])
{
    for (let entry of vars) {
        vars[entry.name] = entry;
    }
    return vars as PathVar[] & Record<string, PathVar>;
}

export class PathTemplate
{
    static readonly Vars = createVarMap([
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
    ]);

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
        return template.replace(/{(.+?)}/g, (g0, g1) => {
            let val = vars[g1];
            return val !== undefined ? this.replaceInvalidPathChars(val) : g0;
        });
    }
    private static replaceInvalidPathChars(str: string)
    {
        const ReplacementChars = {
            '\\': '＼',
            '/': '／',
            ':': '：',
            '*': '＊',
            '?': '？',
            '"': '＂',
            '<': '＜',
            '>': '＞',
            '|': '￤',
        };
        //invalid characters -> similar characters
        str = str.replace(/[\x00-\x1f\/\\:*?"<>|]/g, v => ReplacementChars[v] ?? " ");
        //leading/trailling spaces -> "\u2002 En Space"
        str = str.replace(/(^ +| +$)/g, " ");
        //trailling dots -> "\uFF0E Fullwidth Stop"
        //also handles ".."
        str = str.replace(/\.+$/g, v => "．".repeat(v.length));

        return str;
    }
}

interface TemplateNode
{
    children: TemplateNode[];
    pattern: string;
    literal: boolean;
    id?: string;
}
export class TemplatedSearchTree
{
    root: TemplateNode = {
        children: [],
        pattern: "",
        literal: true
    };
    private _collator = new Intl.Collator(undefined, { sensitivity: "accent", usage: "search" });
    private _template: string[];

    constructor(template: string)
    {
        this._template = template.replace(/\..+$/, "{_ext}").split(/[\/\\]/);
    }

    add(id: string, vars: PathTemplateVars)
    {
        let node = this.root;
        for (let part of this._template) {
            let pattern = PathTemplate.render(part, vars);
            let literal = !/{(.+?)}/.test(pattern);
            if (!literal) { //placeholder is keept for unknown variables
                pattern = pattern.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
                pattern = pattern.replace(/\\{(.+?)\\}/g, (g0, g1) => {
                    if (g1 === "_ext") return "\\.(mp3|m4a|mp4|ogg|opus)$";
                    return PathTemplate.Vars[g1]?.pattern ?? g0;
                });
            }
            node = this.findOrAddChild(node, pattern, literal);
        }
        if (node.id == null) {
            node.id = id;
        } else if (node.id !== id) {
            node.id += ",";
            node.id += id;
        }
    }
    
    private findOrAddChild(node: TemplateNode, pattern: string, isLiteral: boolean)
    {
        for (let child of node.children) {
            if (child.literal === isLiteral && this._collator.compare(child.pattern, pattern) === 0) {
                return child;
            }
        }
        let child = {
            children: [],
            pattern: pattern,
            literal: isLiteral
        };
        node.children.push(child);
        return child;
    }
}