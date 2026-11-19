//
//  sync-demo.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

import { ResourceLoader, SynesthesiaCanvas, SynesthesiaSyncGroup, WavFeeder, loadSynesthesiaModule } from "synesthesia-vibe";

const COMPARE_LOG_INTERVAL = 60;

export class SyncDemo {

    #elements;
    #host;
    #compare;
    #mirror = null;
    #primary = null;
    #group = null;
    #groupFeeder = null;
    #joining = false;
    #compared = 0;
    #mismatches = 0;

    constructor(elements, host, compare) {
        this.#elements = elements;
        this.#host = host;
        this.#compare = compare;
        elements.toggle.addEventListener("change", () => this.#apply());
        elements.scaling.addEventListener("change", () => this.#applyScaling());

        if (compare) {
            elements.stage.classList.add("sync-compare");
            elements.scaling.value = "stretch";
            elements.scaling.disabled = true;
        }

        window.svSyncDemo = {
            get stats() { return { compared: this.compared, mismatches: this.mismatches, grouped: this.grouped }; },
            compared: 0,
            mismatches: 0,
            grouped: false,
            setSync: enabled => this.setSync(enabled),
            setScaling: scaling => this.setScaling(scaling),
            closeGroup: () => this.closeGroup(),
            get group() { return window.svSyncDemo.groupRef(); },
            groupRef: () => this.#group,
            primary: () => this.#primary,
            mirror: () => this.#mirror
        };
    }

    get context() {
        return this.#primary?.syncGroup !== null && this.#primary?.syncGroup !== undefined ? this.#group?.context ?? null : null;
    }

    get isGrouped() {
        return this.#group !== null && this.#primary?.syncGroup === this.#group;
    }

    async start(module, primary, presetPath) {
        this.#primary = primary;
        this.#mirror = await SynesthesiaCanvas.create(this.#elements.mirror, module, {
            configuration: this.#host.configuration(),
            onError: error => this.#host.onError(error),
            syncScaling: this.#elements.scaling.value
        });
        await this.#mirror.loadPreset(presetPath, 0);
        this.#elements.panel.hidden = false;
        this.#setStatus("Independent: each canvas renders on its own");
    }

    adoptPrimary(primary) {
        this.#primary = primary;

        if (this.#group !== null && this.#elements.toggle.checked)
            primary.syncGroup = this.#group;
    }

    async loadPreset(path, blendSeconds) {
        if (this.isGrouped)
            await this.#group.loadPreset(path, blendSeconds);
    }

    setSync(enabled) {
        this.#elements.toggle.checked = enabled;
        return this.#apply();
    }

    closeGroup() {
        this.#group?.close();
        this.#group = null;
        this.#elements.toggle.checked = false;
        window.svSyncDemo.grouped = this.isGrouped;
        console.info("sync group closed: primary=" + this.#primary?.syncGroup + " mirror=" + this.#mirror?.syncGroup);
    }

    setScaling(scaling) {
        this.#elements.scaling.value = scaling;
        this.#applyScaling();
    }

    #applyScaling() {
        const scaling = this.#elements.scaling.value;

        if (this.#mirror !== null)
            this.#mirror.syncScaling = scaling;

        if (this.#primary !== null)
            this.#primary.syncScaling = scaling;
    }

    async #apply() {
        if (this.#joining || this.#mirror === null || this.#primary === null)
            return;

        this.#joining = true;

        try {
            if (this.#elements.toggle.checked)
                await this.#join();
            else
                this.#leave();
        } catch (error) {
            this.#elements.toggle.checked = false;
            this.#leave();
            this.#host.onError(error);
        }

        this.#joining = false;
        window.svSyncDemo.grouped = this.isGrouped;
    }

    async #join() {
        this.#setStatus("Starting the sync group…");

        if (this.#group === null)
            await this.#createGroup();

        await this.#group.loadPreset(this.#host.currentPreset(), 0);
        this.#applyScaling();
        this.#primary.syncGroup = this.#group;
        this.#mirror.syncGroup = this.#group;
        this.#setStatus("Synced: one render, " + this.#group.memberCount + " canvases, " + this.#group.size.width + "x" + this.#group.size.height);
        console.info("sync group joined: members=" + this.#group.memberCount + " size=" + this.#group.size.width + "x" + this.#group.size.height + " scaling=" + this.#elements.scaling.value);
    }

    #leave() {
        if (this.#primary !== null)
            this.#primary.syncGroup = null;

        if (this.#mirror !== null)
            this.#mirror.syncGroup = null;

        this.#setStatus("Independent: each canvas renders on its own");
        console.info("sync group left");
    }

    async #createGroup() {
        const module = await loadSynesthesiaModule();
        const loader = new ResourceLoader(module);
        const loaded = await loader.load(this.#host.manifestUrl());
        const wavPath = loaded.audio.find(path => path.toLowerCase().endsWith(".wav")) ?? null;
        this.#groupFeeder = wavPath !== null ? new WavFeeder(module, wavPath) : null;
        this.#group = await SynesthesiaSyncGroup.create(module, {
            configuration: this.#host.configuration(),
            onError: error => this.#host.onError(error),
            onBeforeRender: (context, deltaSeconds) => this.#groupFeeder?.feed(context, deltaSeconds),
            onFramePresented: this.#compare ? canvases => this.#comparePixels(canvases) : null
        });
        this.#group.setPlaybackRate(this.#host.playbackRate());
    }

    #comparePixels(canvases) {
        if (canvases.length < 2)
            return;

        const reference = canvases[0].toDataURL();
        const identical = canvases.every(canvas => canvas.toDataURL() === reference);
        this.#compared += 1;
        window.svSyncDemo.compared = this.#compared;

        if (!identical) {
            this.#mismatches += 1;
            window.svSyncDemo.mismatches = this.#mismatches;
        }

        if (!identical || this.#compared === 1 || this.#compared % COMPARE_LOG_INTERVAL === 0)
            console.info("sync pixel compare: frame=" + this.#group.frameCount + " canvases=" + canvases.length + " " + canvases.map(canvas => canvas.width + "x" + canvas.height).join(",") + " scaling=" + this.#elements.scaling.value + " identical=" + identical + " compared=" + this.#compared + " mismatches=" + this.#mismatches);
    }

    #setStatus(text) {
        this.#elements.status.textContent = text;
    }
}
