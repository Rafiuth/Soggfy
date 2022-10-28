export default class Utils {
    static padInt(x: number, digits = 2) {
        return Math.floor(x).toString().padStart(digits, '0');
    }

    /**
     * Searches for a path given a root object
     * @param props A set of properties to look for
     * @param path The current path, should be empty when calling this
     * @returns 
     */
    static findPath(obj: any, props: Set<string>, path: string[], visitedObjs: Set<any>) {
        visitedObjs.add(obj);

        for (let key in obj) {
            if (props.has(key)) {
                return true;
            }
            let child = obj[key];
            if (!child || child instanceof String || child instanceof Number || visitedObjs.has(child)) continue;

            path.push(key);
            if (Utils.findPath(child, props, path, visitedObjs)) {
                return path;
            }
            path.pop();
        }
    }
    /** 
     * Gets or sets a nested field specified by the path array
     * Ex: accessObjectPath(obj, ["track","metadata","name"]) -> obj.track.metadata.name
     * @param obj
     * @param path
     * @param newValue Value to set the final field
     */
    static accessObjectPath(obj: any, path: string[], newValue = undefined) {
        let lastField = path.at(-1);
        for (let i = 0; i < path.length - 1; i++) {
            obj = obj[path[i]];
        }
        if (newValue !== undefined) {
            obj[lastField] = newValue;
        }
        return obj[lastField];
    }

    /** Recursively merges source into target. */
    static deepMerge(target: any, source: any) {
        //https://stackoverflow.com/a/34749873
        if (isObject(target) && isObject(source)) {
            for (let key in source) {
                if (isObject(source[key])) {
                    target[key] ??= {};
                    this.deepMerge(target[key], source[key]);
                } else {
                    Object.assign(target, { [key]: source[key] });
                }
            }
        }
        return target;

        function isObject(item: any) {
            return item && typeof item === "object" && !Array.isArray(item);
        }
    }

    static getReactProps(rootElem: Element, targetElem: Element): any {
        const keyof_ReactProps =
            Object.keys(rootElem)
                .find(k => k.startsWith("__reactProps$"));

        //find the path from elem to target
        let path = [];
        let node = targetElem;
        while (node !== rootElem) {
            let parent = node.parentElement;
            let index = 0;
            for (let child of parent.children) {
                if (child[keyof_ReactProps]) index++;
                if (child === node) break;
            }
            path.push({ next: node, index: index });
            node = parent;
        }
        //now find the react state
        let state = node[keyof_ReactProps];
        for (let i = path.length - 1; i >= 0 && state != null; i--) {
            let loc = path[i];

            //find the react state children, ignoring "non element" children
            let childStateIndex = 0;
            let childElemIndex = 0;
            while (childStateIndex < state.children.length) {
                let isElem = state.children[childStateIndex] instanceof Object;
                if (isElem && ++childElemIndex === loc.index) break;
                childStateIndex++;
            }
            let child = state.children[childStateIndex] ?? (childStateIndex === 0 ? state.children : null);
            state = child?.props;
            node = loc.next;
        }
        return state;
    }
}