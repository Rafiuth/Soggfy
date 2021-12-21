type ComponentValueCallback<T> = (key: string, val?: T) => T;

type SliderOptions = {
    min?: number;
    max?: number;
    step?: number;
    formatter?: (x: number) => string;
}
type SwitchButton = HTMLButtonElement & { index: number };

export default class Components
{
    static createSettingOverlay(...elements: Node[])
    {
        let node = document.createElement("div");
        node.style.display = "block";
        node.innerHTML = `
<div class="sgf-settings-overlay">
<div class="sgf-settings-modal" tabindex="1" role="dialog">
    <div class="sgf-settings-container">
        <div class="sgf-settings-header">
            <h1 class="sgf-header-title" as="h1">Soggfy settings</h1>

            <button aria-label="Close" class="sgf-settings-closeBtn">
                <svg width="18" height="18" viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
                    <title>Close</title>
                    <path d="M31.098 29.794L16.955 15.65 31.097 1.51 29.683.093 15.54 14.237 1.4.094-.016 1.508 14.126 15.65-.016 29.795l1.414 1.414L15.54 17.065l14.144 14.143" fill="currentColor" fill-rule="evenodd"></path>
                </svg>
            </button>
        </div>
        <div class="sgf-settings-elements">
        </div>
    </div>
</div>
</div>`;
        node.querySelector(".sgf-settings-elements").append(...elements);
        //@ts-ignore
        node.querySelector(".sgf-settings-closeBtn").onclick = () => node.remove();

        let overlay: HTMLElement = node.querySelector(".sgf-settings-overlay");
        let container: HTMLElement = node.querySelector(".sgf-settings-container");

        //fix for an annoying behavior in onclick():
        //onclick() is fired if we click on the container and then release with the cursor in the overlay.
        //container.onclick = (ev) => ev.stopPropagation(); (doesn't do shit)
        //(I keep wondering if libraries like react deal with things like these painlessly lol)
        let clickedOut = false;

        overlay.onmousedown = (ev) => {
            if (!container.contains(ev.target as Node)) {
                clickedOut = true;
            }
        };
        overlay.onmouseup = (ev) => {
            if (!container.contains(ev.target as Node) && clickedOut) {
                node.remove();
            }
        };
        return node;
    }
    
    static addTopbarButton(title: string, icon: string, callback: () => void)
    {
        let backButton = document.querySelector(".Root__top-bar").querySelector("button");
        let topbarContainer = backButton.parentElement;

        let button = document.createElement("button");
        button.className = backButton.classList[0];
        button.innerHTML = icon;
        button.onclick = callback;
        topbarContainer.append(button);

        return button;
    }

    static toggle(key: string, callback: ComponentValueCallback<boolean>)
    {
        let node = document.createElement("input");
        node.className = "sgf-toggle-switch";
        node.type = "checkbox";
        node.checked = callback(key);

        if (callback) {
            node.onchange = () => callback(key, node.checked);
        }
        return node;
    }
    static select(key: string, options: string[], callback: ComponentValueCallback<string>)
    {
        let node = document.createElement("select");
        node.className = "sgf-select";
        
        for (let i = 0; i < options.length; i++) {
            let opt = document.createElement("option");
            opt.setAttribute("value", i.toString());
            opt.innerText = options[i];

            node.appendChild(opt);
        }
        node.value = options.indexOf(callback(key)).toString();
        
        if (callback) {
            node.onchange = () => callback(key, options[parseInt(node.value)]);
        }
        return node;
    }
    static textInput(key: string, callback: ComponentValueCallback<string>)
    {
        let node = document.createElement("input");
        node.className = "sgf-text-input";
        node.value = callback(key);
        
        if (callback) {
            node.onchange = () => callback(key, node.value);
        }
        return node;
    }

    static slider(key: string, opts: SliderOptions, callback: ComponentValueCallback<number>)
    {
        let initialValue = callback(key);
        let formatter = opts?.formatter ?? (x => x.toString());
        
        let node = document.createElement("div");
        node.className = "sgf-slider-wrapper";
        node.innerHTML = `
<input class="sgf-slider-label"></input>
<input class="sgf-slider" type="range"
    min="${opts?.min ?? 0}" max="${opts?.max ?? 100}" step="${opts?.step ?? 1}" 
    value="${initialValue}">
`;
        let slider: HTMLInputElement = node.querySelector(".sgf-slider");
        let label: HTMLInputElement = node.querySelector(".sgf-slider-label");
        label.value = formatter(initialValue);
        
        let updateLabel = () => label.value = formatter(parseFloat(slider.value));
        slider.oninput = updateLabel;
        
        //remove custom format when editing via the textbox
        label.oninput = () => slider.value = label.value;
        label.onfocus = () => label.value = slider.value;
        label.onblur = updateLabel;
        
        if (callback) {
            let fireCallback = () => callback(key, parseFloat(slider.value));
            label.onchange = fireCallback;
            slider.onchange = fireCallback;
        }
        return node;
    }
    static collapsible(desc: string, ...elements: Node[])
    {
        let node = document.createElement("details");
        node.className = "sgf-collapsible";
        node.innerHTML = `<summary>${desc}</summary>`;
        node.append(...elements);
        return node;
    }
    static tagButton(text: string, callback?: { (): void; })
    {
        let node = document.createElement("button");
        node.className = "sgf-tag-button";
        node.innerText = text;
        node.onclick = callback;
        return node;
    }
    static button(text: string = null, child: string = null, callback: { (): void; })
    {
        let node = document.createElement("button");
        node.className = "sgf-button";
        if (child) node.appendChild(this.parse(child));
        if (text) node.appendChild(new Text(text));
        node.onclick = callback;
        
        return node;
    }

    static switchField(key: string, options: (string | Node | DocumentFragment)[], callback: ComponentValueCallback<number>)
    {
        let node = document.createElement("div");
        node.className = "sgf-switch-field";

        let activeIndex = callback(key, undefined);

        let onButtonClick = (button: SwitchButton) => {
            if (button.index === activeIndex) return;
            activeIndex = button.index;

            for (let btn of node.children) {
                btn.removeAttribute("active");
            }
            button.setAttribute("active", "");

            callback(key, button.index);
        };
        for (let i = 0; i < options.length; i++) {
            let button = document.createElement("button") as SwitchButton;
            button.style.width = (100.0 / options.length) + "%";
            button.index = i;
            button.onclick = () => onButtonClick(button);

            let opt = options[i];
            button.appendChild(
                typeof opt === "string"
                    ? document.createTextNode(opt as string)
                    : opt as Node
            );
            node.appendChild(button);
        }
        node.children[activeIndex].setAttribute("active", "");

        return node;
    }
    static notification(text: string, anchor: Element, fadeDelay = 5, fadeDur = 0.2)
    {
        let node = document.createElement("span");
        node.className = "sgf-notification-bubble";
        node.innerText = text;
        node.style.setProperty("--fade-delay", fadeDelay + "s");
        node.style.setProperty("--fade-dur", fadeDur + "s");

        anchor.parentElement.appendChild(node);

        let rect = node.getBoundingClientRect();
        let anchorRect = anchor.getBoundingClientRect();
        node.style.top = (anchorRect.top - rect.height - 6) + "px";
        node.style.left = (anchorRect.left + (anchorRect.width - rect.width) / 2) + "px";
        node.onanimationend = () => node.remove();

        return node;
    }

    static title(text: string, tagName: "h1" | "h2" | "h3" = "h2")
    {
        let node = document.createElement(tagName);
        node.innerText = text;
        return node;
    }

    static row(desc: string, action: any)
    {
        let node = document.createElement("div");
        node.className = "sgf-setting-row";
        node.innerHTML = `
<label class="col description">${desc}</label>
<div class="col action"></div>`;
        node.querySelector(".action").appendChild(action);
        return node;
    }
    static rows(...elements: (string | Node)[])
    {
        let node = document.createElement("div");
        node.className = "sgf-setting-rows";
        node.append(...elements);
        return node;
    }
    static colDesc(text: string)
    {
        let node = document.createElement("label");
        node.classList.add("col", "description");
        node.innerText = text;
        return node;
    }
    static colSection(...elements: Node[])
    {
        let node = document.createElement("div");
        node.className = "sgf-setting-cols";
        node.append(...elements);
        return node;
    }
    static section(desc: string, ...elements: Node[])
    {
        let node = document.createElement("div");
        node.className = "sgf-setting-section";
        node.innerHTML = `<h2>${desc}</h2>`;
        node.append(...elements);
        return node;
    }
    static subSection(...elements: Node[])
    {
        let node = document.createElement("div");
        node.className = "sgf-setting-section";
        node.style.marginLeft = "20px";
        node.append(...elements);
        return node;
    }

    static parse(html: string): DocumentFragment | Node
    {
        var template = document.createElement("template");
        template.innerHTML = html.trim(); //trim(): Never return a text node of whitespace as the result
        return template.content.childElementCount > 1 ? template.content : template.content.firstChild;
    }
}