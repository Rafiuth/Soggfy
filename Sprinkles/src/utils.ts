type Detour = (stage: "pre" | "post", args: any[], ret?: any) => void;

export default class Utils
{
    static createHook(obj: any, funcName: string, detour: Detour)
    {
        //https://stackoverflow.com/a/62333813
        let originalFunc = obj[funcName];
        obj[funcName] = function (...args: any[]) {
            detour("pre", args);

            let ret = originalFunc.apply(this, args);

            if (ret instanceof Promise) {
                return ret.then(val => {
                    detour("post", args, val);
                    return val;
                });
            } else {
                detour("post", args, ret);
                return ret;
            }
        }
    }
    static padInt2(x: number, digits = 2)
    {
        return Math.floor(x).toString().padStart(digits ?? 2, '0');
    }

    /**
     * Searches for a path given a root object
     * @param props A set of properties to look for
     * @param path The current path, should be empty when calling this
     * @returns 
     */
    static findPath(obj: any, props: Set<string>, path: string[], visitedObjs: Set<any>)
    {
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
    static accessObjectPath(obj: any, path: string[], newValue = undefined)
    {
        let lastField = path.at(-1);
        for (let i = 0; i < path.length - 1; i++) {
            obj = obj[path[i]];
        }
        if (newValue !== undefined) {
            obj[lastField] = newValue;
        }
        return obj[lastField];
    }
    static getReactProps(rootElem: Element, targetElem: Element): any
    {
        const keyof_ReactEventHandlers =
            Object.keys(rootElem)
                  .find(k => k.startsWith("__reactEventHandlers$"));
        
        //find the path from elem to target
        let path = [];
        let node = targetElem;
        while (node !== rootElem) {
            let parent = node.parentElement;
            let index = 0;
            for (let child of parent.children) {
                if (child[keyof_ReactEventHandlers]) index++;
                if (child === node) break;
            }
            path.push({ next: node, index: index });
            node = parent;
        }
        //now find the react state
        let state = node[keyof_ReactEventHandlers];
        for (let i = path.length - 1; i >= 0; i--) {
            let loc = path[i];
            
            //find the react state children, ignoring "non element" children
            let childStateIndex = 0;
            let childElemIndex = 0;
            while (childStateIndex < state.children.length) {
                let isElem = state.children[childStateIndex] instanceof Object;
                if (isElem && ++childElemIndex === loc.index) break;
                childStateIndex++;
            }
            state = state.children[childStateIndex].props;
            node = loc.next;
        }
        return state;
    }
}