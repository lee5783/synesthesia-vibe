//
//  main.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { AudioInput, ResourceLoader, SynesthesiaCanvas, SynesthesiaContext, SynesthesiaError, WavFeeder, loadSynesthesiaModule } from "synesthesia-vibe";
import { runCapture } from "./capture.js";

const MANIFEST_URL = "resources/manifest.json";
const PRESET_BLEND_SECONDS = 2;
const METER_FULL_SCALE = 3;
const DEBUG_INTERVAL_DEFAULT = 1;
const AUTO_CHANGE_TICK_MS = 1000;
const DEFAULT_AUTO_INTERVAL_SECONDS = 30;
const DEFAULT_FPS = 60;
const DEFAULT_FOLDER_NAME = "mvp-presets";
const PRESET_EXTENSION = ".milk";
const TEXTURE_EXTENSIONS = Object.freeze(["jpg", "jpeg", "jfif", "png", "webp", "tga", "bmp", "dib", "gif"]);
const CONFIGURATION_PARAMETERS = Object.freeze(["maxFps", "maxLuminance", "flashLimit"]);
const SOURCES = Object.freeze(["none", "wav", "microphone", "system"]);
const SOURCE_LABELS = Object.freeze({ none: "None", wav: "Bundled WAV", microphone: "Microphone", system: "System sound" });
const SOURCE_HINTS = Object.freeze({
    none: "Presets animate without audio",
    wav: "Loops the sample track that ships with the demo",
    microphone: "Live input from your mic",
    system: "Audio from a shared tab or screen"
});
const PERMISSION_TEXT = Object.freeze({
    microphone: "Your browser will ask for microphone access. Needs HTTPS or localhost.",
    system: "Pick a tab or screen and turn on “Share audio” when your browser asks."
});
const WAVE_ICON = "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.75\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\"><path d=\"M4 10v4\"></path><path d=\"M8 7v10\"></path><path d=\"M12 4v16\"></path><path d=\"M16 7v10\"></path><path d=\"M20 10v4\"></path></svg>";

class SampleApp {

    #elements;
    #configuration;
    #loader = null;
    #module = null;
    #view = null;
    #presets = [];
    #presetIndex = 0;
    #presetName = "";
    #folderName = DEFAULT_FOLDER_NAME;
    #wavPath = null;
    #wavFeeder = null;
    #audioInput = null;
    #source = "none";
    #sourceError = { source: null, message: "" };
    #available = { none: true, wav: false, microphone: false, system: false };
    #paused = false;
    #loadFailed = false;
    #busy = false;
    #switching = Promise.resolve();
    #playbackRate = 1;
    #validatePath = null;
    #debugEnabled = false;
    #debugInterval = DEBUG_INTERVAL_DEFAULT;
    #autoEnabled = false;
    #autoInterval = DEFAULT_AUTO_INTERVAL_SECONDS;
    #autoElapsed = 0;
    #loadSerial = 0;
    #requestedIndex = 0;
    #autoTimer = 0;
    #glVersion = "";

    constructor(elements, configuration, playbackRate = 1, validatePath = null) {
        this.#elements = elements;
        this.#configuration = { ...configuration };
        this.#playbackRate = playbackRate;
        this.#validatePath = validatePath;
    }

    async run() {
        this.#listen();
        this.#seedFps();
        this.#renderSources();
        this.#updateControls();

        try {
            this.#module = await loadSynesthesiaModule();
            this.#setLoadStatus("Loading resources…");
            this.#loader = new ResourceLoader(this.#module);
            const loaded = await this.#loader.load(MANIFEST_URL, progress => this.#showProgress(progress));
            this.#wavPath = loaded.audio.find(path => path.toLowerCase().endsWith(".wav")) ?? null;

            if (loaded.presets.length === 0)
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, MANIFEST_URL + " lists no presets");

            await this.#createView();
            this.#validateRequested();
            this.#updateAvailability();
            this.#renderSources();
            this.#setPresets(loaded.presets);
            this.#setLoadStatus("Ready: " + this.#presets.length + " presets, " + (loaded.bytes / 1048576).toFixed(1) + " MB");
            this.#elements.loadPanel.hidden = true;
            this.#loadPreset(0, 0);
            await this.#activateSource("wav", "none");
            console.info("playback rate: " + this.#view.playbackRate);
        } catch (error) {
            this.#loadFailed = true;
            this.#setLoadStatus("Loading failed");
            this.showError(error);
        }
    }

    async #createView() {
        const view = await SynesthesiaCanvas.create(this.#elements.canvas, this.#module, {
            configuration: { ...this.#configuration, maxFps: this.#fpsSetting() },
            onError: error => this.showError(error),
            onBeforeRender: (context, deltaSeconds) => this.#wavFeeder?.feed(context, deltaSeconds)
        });

        view.setPlaybackRate(this.#playbackRate);
        view.setDebugInterval(this.#debugInterval);
        view.onDebugInfo = this.#debugEnabled ? info => this.#showDebugInfo(info) : null;

        if (this.#paused)
            view.pause();

        this.#view = view;
        this.#glVersion = "";
        return view;
    }

    showError(error) {
        const message = error instanceof Error ? error.message : String(error);
        console.error(error);
        this.#elements.errorText.textContent = message;
        this.#elements.errorBanner.hidden = false;
    }

    hideError() {
        this.#elements.errorBanner.hidden = true;
    }

    #listen() {
        const elements = this.#elements;
        elements.previousPreset.addEventListener("click", () => this.#stepPreset(-1));
        elements.nextPreset.addEventListener("click", () => this.#stepPreset(1));
        elements.playButton.addEventListener("click", () => this.#togglePause());
        elements.chooseFolder.addEventListener("click", () => elements.folderInput.click());
        elements.folderInput.addEventListener("change", () => this.#loadFolder(elements.folderInput));
        elements.fullscreenButton.addEventListener("click", () => this.#toggleFullscreen());
        elements.autoChange.addEventListener("click", () => this.#toggleAutoChange());
        elements.interval.addEventListener("change", () => this.#changeAutoInterval(Number(elements.interval.value)));
        elements.fps.addEventListener("change", () => this.#changeFps(Number(elements.fps.value)));
        elements.debugSwitch.addEventListener("click", () => this.#toggleDebug());
        elements.debugButton.addEventListener("click", () => this.#toggleDebug());
        elements.errorDismiss.addEventListener("click", () => this.hideError());

        for (const source of SOURCES) {
            const input = elements.sourceRows[source].querySelector("input");
            input.addEventListener("change", () => {
                if (input.checked)
                    this.#selectSource(source);
            });
        }

        document.addEventListener("keydown", event => this.#handleKey(event));
        document.addEventListener("fullscreenchange", () => this.#updateControls());
    }

    #showProgress({ loaded, total, path }) {
        if (this.#loadFailed)
            return;

        this.#elements.loadProgress.max = total;
        this.#elements.loadProgress.value = loaded;
        this.#setLoadStatus("Loading resources " + loaded + "/" + total + ": " + path.split("/").pop());
    }

    #setLoadStatus(text) {
        this.#elements.loadStatus.textContent = text;
    }

    #seedFps() {
        const raw = this.#configuration.maxFps;

        if (!Number.isFinite(raw))
            return;

        const value = raw <= 0 ? 0 : raw;
        const select = this.#elements.fps;
        const existing = [...select.options].find(option => Number(option.value) === value);

        if (existing !== undefined) {
            select.value = existing.value;
            return;
        }

        select.append(new Option(value === 0 ? "Uncapped" : value + " fps", String(value), true, true));
        select.value = String(value);
    }

    #setPresets(presets) {
        this.#presets = [...presets].sort();
        this.#renderPlaylist();
    }

    #renderPlaylist() {
        const rows = this.#presets.map((path, index) => {
            const current = index === this.#presetIndex;
            const item = document.createElement("li");
            const button = document.createElement("button");
            button.type = "button";
            button.className = current ? "row current" : "row";
            button.setAttribute("aria-label", "Play preset " + presetName(path));

            if (current)
                button.setAttribute("aria-current", "true");

            const number = document.createElement("span");
            number.className = "row-number";
            number.textContent = String(index + 1);
            const name = document.createElement("span");
            name.className = "row-name";
            name.textContent = presetName(path);
            button.append(number, name);

            if (current) {
                const icon = document.createElement("span");
                icon.className = "row-icon";
                icon.innerHTML = WAVE_ICON;
                button.append(icon);
            }

            button.addEventListener("click", () => this.#loadPreset(index, PRESET_BLEND_SECONDS));
            item.append(button);
            return item;
        });

        this.#elements.playlist.replaceChildren(...rows);
        this.#elements.presetCount.textContent = this.#presets.length + (this.#presets.length === 1 ? " preset" : " presets");
    }

    #updateAvailability() {
        this.#available = {
            none: true,
            wav: this.#wavPath !== null,
            microphone: AudioInput.isMicrophoneSupported(),
            system: AudioInput.isDisplayCaptureSupported()
        };

        const names = SOURCES.filter(name => this.#available[name]);
        console.info("audio sources available: " + names.join(", "));
    }

    #renderSources() {
        for (const source of SOURCES) {
            const row = this.#elements.sourceRows[source];
            const input = row.querySelector("input");
            const unavailable = !this.#available[source];
            row.classList.toggle("disabled", unavailable);
            row.classList.toggle("selected", source === this.#source);
            input.disabled = unavailable;
            input.checked = source === this.#source;
        }

        this.#renderSourceHints();
        this.#renderCallout(this.#source);
    }

    #renderSourceHints() {
        for (const source of SOURCES) {
            const hint = this.#elements.sourceHints[source];
            hint.textContent = this.#sourceError.source === source
                ? this.#sourceError.message
                : this.#available[source] ? SOURCE_HINTS[source] : SOURCE_HINTS[source] + " (" + unavailableReason(source) + ")";
        }
    }

    #renderCallout(source) {
        const shown = source === "microphone" || source === "system";
        this.#elements.permissionCallout.hidden = !shown;
        this.#elements.permissionText.textContent = shown ? PERMISSION_TEXT[source] : "";
    }

    #checkSourceRadio() {
        for (const source of SOURCES)
            this.#elements.sourceRows[source].querySelector("input").checked = source === this.#source;
    }

    #selectSource(source) {
        if (this.#busy || this.#view === null || !this.#available[source]) {
            this.#checkSourceRadio();
            return this.#switching;
        }

        const previous = this.#source;
        this.#switching = this.#switching.then(() => this.#activateSource(source, previous));
        return this.#switching;
    }

    async #activateSource(source, previous) {
        try {
            this.#stopSource();

            switch (source) {
                case "none":
                    break;
                case "wav":
                    if (this.#wavPath === null)
                        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "no bundled WAV in " + MANIFEST_URL);
                    this.#wavFeeder = new WavFeeder(this.#module, this.#wavPath);
                    break;
                case "microphone":
                    this.#audioInput = await AudioInput.fromMicrophone(() => this.#view?.context);
                    break;
                case "system":
                    this.#audioInput = await AudioInput.fromDisplayCapture(() => this.#view?.context);

                    if (this.#audioInput === null)
                        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "system sound capture is not supported in this browser");

                    break;
                default:
                    throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "unknown audio source " + source);
            }

            this.#source = source;
            this.#sourceError = { source: null, message: "" };
            this.hideError();
        } catch (error) {
            this.#stopSource();
            this.#source = previous;
            this.#sourceError = { source, message: error instanceof Error ? error.message : String(error) };
            this.showError(error);
        }

        this.#renderSources();
        this.#updateControls();
    }

    #stopSource() {
        this.#audioInput?.stop();
        this.#audioInput = null;
        this.#wavFeeder?.close();
        this.#wavFeeder = null;
    }

    #stepPreset(step) {
        if (this.#presets.length === 0)
            return;

        const count = this.#presets.length;
        this.#loadPreset((this.#requestedIndex + step + count) % count, PRESET_BLEND_SECONDS);
    }

    #logPresetInfo(view) {
        const info = view.presetInfo();
        console.info("preset info: " + this.#presetName + " version=" + info.preset_version
            + " warpPs=" + info.warp_ps_version + " compPs=" + info.comp_ps_version
            + " warpShader=" + info.warp_shader + " compShader=" + info.comp_shader
            + " fallback=\"" + info.fallback_reason + "\"");
    }

    async #loadPreset(index, blendSeconds) {
        if (this.#view === null || this.#busy || this.#presets[index] === undefined)
            return;

        const path = this.#presets[index];
        const serial = ++this.#loadSerial;
        this.#requestedIndex = index;
        const view = this.#view;

        try {
            const timing = await view.loadPreset(path, blendSeconds);

            if (serial !== this.#loadSerial || view !== this.#view)
                return;

            this.#presetIndex = index;
            this.#presetName = presetName(path);

            if (timing !== null) {
                this.#reportLastError(this.#presetName);
                console.info("preset prepared in " + timing.prepareMs.toFixed(0) + " ms: " + this.#presetName);
                console.info("preset loaded in " + timing.commitMs.toFixed(0) + " ms: " + this.#presetName);
                this.#logPresetInfo(view);
            }
        } catch (error) {
            if (serial === this.#loadSerial)
                this.showError(error);
        }

        if (serial !== this.#loadSerial)
            return;

        this.#autoElapsed = 0;
        this.#renderPlaylist();
        this.#updateControls();
    }

    #reportLastError(label) {
        const text = this.#view?.context?.lastError() ?? "";

        if (text !== "")
            this.showError(new SynesthesiaError(SynesthesiaError.SV_ERR_PARSE, label + ": " + text));
        else
            this.hideError();
    }

    async #loadFolder(input) {
        const files = [...(input.files ?? [])];
        input.value = "";

        if (files.length === 0 || this.#loader === null || this.#busy)
            return;

        const presets = files.filter(file => file.name.toLowerCase().endsWith(PRESET_EXTENSION));
        const textures = files.filter(file => !file.name.toLowerCase().endsWith(PRESET_EXTENSION) && TEXTURE_EXTENSIONS.includes(extensionOf(file.name)));

        if (presets.length === 0) {
            this.showError(new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "no " + PRESET_EXTENSION + " presets in the chosen folder"));
            return;
        }

        this.#busy = true;

        try {
            for (const file of textures)
                this.#loader.writeFile("textures", file.name, new Uint8Array(await file.arrayBuffer()));

            const paths = [];

            for (const file of presets)
                paths.push(this.#loader.writeFile("presets", file.name, new Uint8Array(await file.arrayBuffer())));

            this.#folderName = topFolderName(files[0]) ?? DEFAULT_FOLDER_NAME;
            this.#elements.folderName.textContent = this.#folderName;
            this.#setPresets(paths);
            console.info("folder loaded: " + this.#folderName + " (" + paths.length + " presets, " + textures.length + " textures)");
        } catch (error) {
            this.showError(error);
        } finally {
            this.#busy = false;
        }

        this.#autoElapsed = 0;
        this.#loadPreset(0, PRESET_BLEND_SECONDS);
    }

    #validateRequested() {
        if (this.#validatePath === null)
            return;

        const path = this.#validatePath === "" ? null : this.#validatePath;

        try {
            if (path === null)
                SynesthesiaContext.validatePresetText(this.#module, "");
            else
                SynesthesiaContext.validatePreset(this.#module, path);
            console.info("validate " + (path ?? "(empty text)") + ": SV_OK");
        } catch (error) {
            console.info("validate " + (path ?? "(empty text)") + ": " + (error instanceof SynesthesiaError ? error.message : String(error)));
        }
    }

    #handleKey(event) {
        if (event.target instanceof HTMLSelectElement || event.target instanceof HTMLInputElement || event.target instanceof HTMLButtonElement)
            return;

        switch (event.key) {
            case "ArrowLeft":
                this.#stepPreset(-1);
                event.preventDefault();
                break;
            case "ArrowRight":
                this.#stepPreset(1);
                event.preventDefault();
                break;
            case " ":
                this.#togglePause();
                event.preventDefault();
                break;
            case "f":
            case "F":
                this.#toggleFullscreen();
                break;
            default:
                break;
        }
    }

    #togglePause() {
        if (this.#view === null || this.#busy)
            return;

        this.#paused = !this.#paused;
        this.#paused ? this.#view.pause() : this.#view.resume();
        console.info(this.#paused ? "paused" : "resumed");
        this.#updateControls();
    }

    #toggleFullscreen() {
        const request = document.fullscreenElement !== null
            ? document.exitFullscreen()
            : this.#elements.visualizerSection.requestFullscreen();
        request.catch(error => this.showError(new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "fullscreen unavailable: " + error.message)));
    }

    #toggleAutoChange() {
        this.#autoEnabled = !this.#autoEnabled;
        this.#autoElapsed = 0;
        console.info("auto-change: " + (this.#autoEnabled ? "on" : "off"));
        this.#applyAutoChange();
        this.#updateControls();
    }

    #changeAutoInterval(seconds) {
        this.#autoInterval = seconds;
        this.#autoElapsed = 0;
        console.info("auto-change interval: " + seconds + " s");
        this.#updateControls();
    }

    #applyAutoChange() {
        if (this.#autoTimer !== 0) {
            clearInterval(this.#autoTimer);
            this.#autoTimer = 0;
        }

        if (this.#autoEnabled)
            this.#autoTimer = setInterval(() => this.#tickAutoChange(), AUTO_CHANGE_TICK_MS);
    }

    #tickAutoChange() {
        if (!this.#autoEnabled || this.#paused)
            return;

        this.#autoElapsed += 1;

        if (this.#autoElapsed >= this.#autoInterval) {
            const count = this.#presets.length;
            this.#autoElapsed = 0;

            if (count > 0)
                this.#loadPreset((this.#requestedIndex + 1) % count, PRESET_BLEND_SECONDS);

            return;
        }

        this.#updateControls();
    }

    async #changeFps(value) {
        if (this.#view === null || this.#busy)
            return;

        this.#busy = true;
        this.#updateControls();
        console.info("frame rate: " + value + " fps, recreating the visualizer");

        try {
            this.#view.close();
            this.#view = null;
            await this.#createView();
            this.#busy = false;
            this.#loadPreset(this.#presetIndex, 0);
            console.info("frame rate: recreated, gl=" + (this.#glString() || "(pending)"));
        } catch (error) {
            this.#busy = false;
            this.showError(error);
        }

        this.#updateControls();
    }

    #fpsSetting() {
        const value = Number(this.#elements.fps.value);
        return Number.isFinite(value) ? value : DEFAULT_FPS;
    }

    #fpsLabel() {
        const value = this.#fpsSetting();
        return value === 0 ? "Uncapped" : value + " fps";
    }

    #toggleDebug() {
        this.#debugEnabled = !this.#debugEnabled;

        if (this.#view !== null)
            this.#view.onDebugInfo = this.#debugEnabled ? info => this.#showDebugInfo(info) : null;

        this.#elements.debugCard.hidden = !this.#debugEnabled;

        if (this.#debugEnabled)
            this.#resetDebugCard();

        console.info("debug info: " + (this.#debugEnabled ? "on" : "off"));
        this.#updateControls();
    }

    #showDebugInfo(info) {
        const elements = this.#elements;
        const fps = Number(info.display_fps) || 0;
        const cap = this.#fpsSetting();
        elements.debugFps.textContent = fps.toFixed(1);
        elements.debugFps.classList.toggle("warning", fps < 50);
        elements.debugCap.textContent = cap === 0 ? "none" : String(cap);
        elements.debugMs.textContent = fps > 0 ? (1000 / fps).toFixed(1) : "--";
        elements.debugPreset.textContent = info.preset_name || this.#presetName;
        elements.debugSize.textContent = info.width + "×" + info.height + " @1.0";
        elements.debugGl.textContent = this.#glString() || "WebGL2";
        elements.debugAudio.textContent = this.#audioLabel();
        elements.debugRate.textContent = "1.0× · Blend";
        elements.debugAuto.textContent = this.#autoText();
        setBar(elements.debugBass, info.bass);
        setBar(elements.debugMid, info.mid);
        setBar(elements.debugTreble, info.treb);
        const failed = (info.last_error ?? "") !== "";
        elements.debugError.hidden = !failed;
        elements.debugErrorLabel.hidden = !failed;
        elements.debugError.textContent = failed ? info.last_error : "";
    }

    #resetDebugCard() {
        const elements = this.#elements;
        elements.debugFps.textContent = "--";
        elements.debugFps.classList.remove("warning");
        elements.debugCap.textContent = this.#fpsSetting() === 0 ? "none" : String(this.#fpsSetting());
        elements.debugMs.textContent = "--";
        elements.debugPreset.textContent = this.#presetName;
        elements.debugSize.textContent = "--";
        elements.debugGl.textContent = this.#glString() || "WebGL2";
        elements.debugAudio.textContent = this.#audioLabel();
        elements.debugRate.textContent = "1.0× · Blend";
        elements.debugAuto.textContent = this.#autoText();
        setBar(elements.debugBass, 0);
        setBar(elements.debugMid, 0);
        setBar(elements.debugTreble, 0);
        elements.debugError.hidden = true;
        elements.debugErrorLabel.hidden = true;
        elements.debugError.textContent = "";
    }

    #autoText() {
        return this.#autoEnabled ? "every " + this.#autoInterval + " s · next " + Math.max(this.#autoInterval - this.#autoElapsed, 0) + " s" : "off";
    }

    #glString() {
        if (this.#glVersion === "" && this.#view !== null) {
            const match = /(?:^|\n)GL_VERSION=(.*)/.exec(this.#view.glStrings());
            this.#glVersion = match === null ? "" : match[1].trim();
        }

        return this.#glVersion;
    }

    #audioLabel() {
        if (this.#source === "none")
            return "none";

        const rate = this.#source === "wav" ? this.#wavFeeder?.sampleRate ?? 0 : this.#audioInput?.inputSampleRate ?? 0;
        return SOURCE_LABELS[this.#source] + " · " + (rate / 1000).toFixed(1) + " kHz";
    }

    #updateControls() {
        const elements = this.#elements;
        const ready = this.#view !== null;
        const blocked = !ready || this.#busy;

        elements.playButton.disabled = blocked;
        elements.previousPreset.disabled = blocked;
        elements.nextPreset.disabled = blocked;
        elements.chooseFolder.disabled = this.#busy;
        elements.fps.disabled = blocked;
        elements.autoChange.classList.toggle("on", this.#autoEnabled);
        elements.autoChange.setAttribute("aria-checked", String(this.#autoEnabled));
        elements.interval.disabled = !this.#autoEnabled;
        elements.intervalRow.classList.toggle("disabled", !this.#autoEnabled);
        elements.debugSwitch.classList.toggle("on", this.#debugEnabled);
        elements.debugSwitch.setAttribute("aria-checked", String(this.#debugEnabled));
        elements.debugButton.classList.toggle("on", this.#debugEnabled);
        elements.debugButton.setAttribute("aria-pressed", String(this.#debugEnabled));
        elements.playButton.setAttribute("aria-label", this.#paused ? "Play" : "Pause");
        elements.pauseIcon.hidden = this.#paused;
        elements.playIcon.hidden = !this.#paused;
        this.#updateStatusPill();
        this.#updateNowPlaying();
    }

    #updateStatusPill() {
        const elements = this.#elements;
        const source = SOURCE_LABELS[this.#source] ?? SOURCE_LABELS.none;
        const state = this.#busy ? "Restarting" : this.#paused ? "Paused" : "Playing";
        elements.statusText.textContent = this.#view === null && !this.#loadFailed
            ? "Loading…"
            : state + " · " + this.#fpsLabel() + " · " + source;
        elements.statusDot.classList.toggle("paused", this.#paused || this.#busy);
    }

    #updateNowPlaying() {
        const elements = this.#elements;
        const count = this.#presets.length;
        elements.nowPlaying.textContent = this.#presetName;
        const position = count === 0 ? "Preset 0 of 0" : "Preset " + (this.#presetIndex + 1) + " of " + count;
        const auto = this.#autoEnabled ? "Next preset in " + Math.max(this.#autoInterval - this.#autoElapsed, 0) + " s" : "Auto-change off";
        elements.nowSubtitle.textContent = position + " · " + auto;
    }
}

function presetName(path) {
    return path.split("/").pop().replace(/\.milk$/i, "");
}

function extensionOf(name) {
    const index = name.lastIndexOf(".");
    return index < 0 ? "" : name.slice(index + 1).toLowerCase();
}

function topFolderName(file) {
    const segment = (file.webkitRelativePath ?? "").split("/")[0];
    return segment === "" ? null : segment;
}

function unavailableReason(source) {
    if (globalThis.isSecureContext !== true)
        return "needs HTTPS or localhost";

    if (source === "system" && typeof navigator.mediaDevices?.getDisplayMedia === "function")
        return "Chromium only";

    return "unavailable";
}

function setBar(element, level) {
    const safe = Number.isFinite(level) ? Math.max(level, 0) : 0;
    element.style.width = (Math.min(safe / METER_FULL_SCALE, 1) * 100).toFixed(1) + "%";
}

function elementsById() {
    const byId = id => document.getElementById(id);
    return {
        app: byId("app"),
        canvas: byId("visualizer"),
        visualizerSection: byId("visualizer-section"),
        errorBanner: byId("error-banner"),
        errorText: byId("error-text"),
        errorDismiss: byId("error-dismiss"),
        loadPanel: byId("load-panel"),
        loadStatus: byId("load-status"),
        loadProgress: byId("load-progress"),
        statusText: byId("status-text"),
        statusDot: byId("status-dot"),
        playlist: byId("playlist"),
        presetCount: byId("preset-count"),
        folderName: byId("folder-name"),
        folderInput: byId("folder-input"),
        chooseFolder: byId("choose-folder"),
        previousPreset: byId("previous-preset"),
        nextPreset: byId("next-preset"),
        playButton: byId("play-button"),
        fullscreenButton: byId("fullscreen-button"),
        pauseIcon: byId("play-button").querySelector(".pause-icon"),
        playIcon: byId("play-button").querySelector(".play-icon"),
        nowPlaying: byId("now-playing"),
        nowSubtitle: byId("now-subtitle"),
        autoChange: byId("auto-change"),
        interval: byId("interval"),
        intervalRow: byId("interval-row"),
        fps: byId("fps"),
        transition: byId("transition"),
        debugSwitch: byId("debug-switch"),
        debugButton: byId("debug-button"),
        debugCard: byId("debug-card"),
        debugFps: byId("debug-fps"),
        debugCap: byId("debug-cap"),
        debugMs: byId("debug-ms"),
        debugPreset: byId("debug-preset"),
        debugSize: byId("debug-size"),
        debugGl: byId("debug-gl"),
        debugAudio: byId("debug-audio"),
        debugRate: byId("debug-rate"),
        debugAuto: byId("debug-auto"),
        debugBass: byId("debug-bass"),
        debugMid: byId("debug-mid"),
        debugTreble: byId("debug-treble"),
        debugError: byId("debug-error"),
        debugErrorLabel: byId("debug-error-label"),
        permissionCallout: byId("permission-callout"),
        permissionText: byId("permission-text"),
        sourceRows: {
            none: byId("row-none"),
            wav: byId("row-wav"),
            microphone: byId("row-microphone"),
            system: byId("row-system")
        },
        sourceHints: {
            none: byId("hint-none"),
            wav: byId("hint-wav"),
            microphone: byId("hint-microphone"),
            system: byId("hint-system")
        },
        capturePanel: byId("capture-panel"),
        captureStatus: byId("capture-status"),
        captureFiles: byId("capture-files")
    };
}

function numericParameters(parameters, names) {
    return Object.fromEntries(names.filter(name => parameters.has(name)).map(name => [name, Number(parameters.get(name))]));
}

const elements = elementsById();
const parameters = new URLSearchParams(location.search);

if (parameters.has("capture")) {
    elements.app.hidden = true;
    elements.capturePanel.hidden = false;
    runCapture(parameters, elements);
} else {
    const app = new SampleApp(elements, numericParameters(parameters, CONFIGURATION_PARAMETERS), numericParameters(parameters, ["playbackRate"]).playbackRate ?? 1, parameters.get("validate"));
    window.addEventListener("error", event => app.showError(event.error ?? event.message));
    window.addEventListener("unhandledrejection", event => app.showError(event.reason));
    app.run();
}
