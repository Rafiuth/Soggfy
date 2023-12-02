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

    static getReactProps(parent: Element, target: Element): any {
        const keyof_ReactProps = Object.keys(parent).find(k => k.startsWith("__reactProps$"));
        const symof_ReactFragment = Symbol.for("react.fragment");

        //Find the path from target to parent
        let path = [];
        for (let elem = target; elem !== parent;) {
            let index = 0;
            for (let sibling = elem; sibling != null;) {
                if (sibling[keyof_ReactProps]) index++;
                sibling = sibling.previousElementSibling;
            }
            path.push(index);
            elem = elem.parentElement;
        }
        //Walk down the path to find the react state props
        let state = parent[keyof_ReactProps];
        for (let i = path.length - 1; i >= 0 && state != null; i--) {
            if (state.children.type == symof_ReactFragment) {
                state = state.children.props;
            }
            //Find the target child state index
            let childStateIndex = 0, childElemIndex = 0;
            while (childStateIndex < state.children.length) {
                let childState = state.children[childStateIndex];
                if (childState instanceof Object) {
                    //Fragment children are inlined in the parent DOM element
                    let isFragment = childState.type === symof_ReactFragment && childState.props.children.length;
                    childElemIndex += isFragment ? childState.props.children.length : 1;
                    if (childElemIndex === path[i]) break;
                }
                childStateIndex++;
            }
            let childState = state.children[childStateIndex] ?? (childStateIndex === 0 ? state.children : null);
            state = childState?.props;
        }
        return state;
    }
}