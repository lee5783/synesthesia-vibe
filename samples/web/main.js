//
//  main.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { AudioInput, ResourceLoader, SynesthesiaCanvas, SynesthesiaError, WavFeeder, loadSynesthesiaModule } from "synesthesia-vibe";
import { runCapture } from "./capture.js";

const MANIFEST_URL = "resources/manifest.json";
const PRESET_BLEND_SECONDS = 2;
const METER_FULL_SCALE = 3;
const CUSTOM_PRESET_VALUE = "custom";
const PRESET_SECTION = /^\s*\[preset\d+\]\s*$/im;
const PRESET_KEY = /^\s*(MILKDROP_PRESET_VERSION|PSVERSION|fRating|fGammaAdj|fDecay|zoom|rot|warp|per_frame_\d+|per_pixel_\d+|wave_\d+_\w+|shapecode_\d+_\w+|warp_\d+|comp_\d+)\s*=/im;

class SampleApp {

    #elements;
    #module = null;
    #view = null;
    #presets = [];
    #presetIndex = 0;
    #presetName = "";
    #wavPath = null;
    #wavFeeder = null;
    #audioInput = null;
    #source = "wav";
    #started = false;
    #loadFailed = false;
    #switching = Promise.resolve();

    constructor(elements) {
        this.#elements = elements;
    }

    async run() {
        this.#listen();

        try {
            this.#module = await loadSynesthesiaModule();
            this.#setLoadStatus("Loading resources…");
            const loaded = await new ResourceLoader(this.#module).load(MANIFEST_URL, progress => this.#showProgress(progress));
            this.#presets = loaded.presets;
            this.#wavPath = loaded.audio.find(path => path.toLowerCase().endsWith(".wav")) ?? null;

            if (this.#presets.length === 0)
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, MANIFEST_URL + " lists no presets");

            this.#view = await SynesthesiaCanvas.create(this.#elements.canvas, this.#module, {
                onError: error => this.showError(error),
                onBeforeRender: (context, deltaSeconds) => this.#wavFeeder?.feed(context, deltaSeconds)
            });
            this.#populatePresets();
            this.#populateSources();
            this.#loadPreset(0, 0);
            this.#setLoadStatus("Ready: " + this.#presets.length + " presets, " + (loaded.bytes / 1048576).toFixed(1) + " MB");
            this.#elements.startButton.disabled = false;
            this.#elements.startButton.focus();
            this.#updateMeters();
        } catch (error) {
            this.#loadFailed = true;
            this.#setLoadStatus("Loading failed");
            this.showError(error);
        }
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
        elements.startButton.addEventListener("click", () => this.#start());
        elements.previousPreset.addEventListener("click", () => this.#stepPreset(-1));
        elements.nextPreset.addEventListener("click", () => this.#stepPreset(1));
        elements.presetSelect.addEventListener("change", () => this.#loadPreset(Number(elements.presetSelect.value), PRESET_BLEND_SECONDS));
        elements.loadMilk.addEventListener("click", () => elements.milkInput.click());
        elements.milkInput.addEventListener("change", () => this.#loadMilkFile(elements.milkInput));
        elements.sourceSelect.addEventListener("change", () => this.#selectSource(elements.sourceSelect.value));
        elements.audioFileInput.addEventListener("change", () => this.#playAudioFile(elements.audioFileInput));
        elements.fullscreenButton.addEventListener("click", () => this.#toggleFullscreen());
        elements.errorDismiss.addEventListener("click", () => this.hideError());
        document.addEventListener("keydown", event => this.#handleKey(event));
        document.addEventListener("fullscreenchange", () => {
            elements.fullscreenButton.textContent = document.fullscreenElement !== null ? "Exit fullscreen" : "Fullscreen";
        });
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

    #populatePresets() {
        const select = this.#elements.presetSelect;
        select.replaceChildren(...this.#presets.map((path, index) => new Option(presetName(path), String(index))));
    }

    #populateSources() {
        const select = this.#elements.sourceSelect;
        const available = {
            wav: this.#wavPath !== null,
            microphone: AudioInput.isMicrophoneSupported(),
            file: AudioInput.isMediaElementSupported(),
            tab: AudioInput.isDisplayCaptureSupported()
        };

        for (const option of [...select.options]) {
            if (option.value === "tab" && !available.tab) {
                option.remove();
                continue;
            }

            if (!available[option.value]) {
                option.disabled = true;
                option.textContent += " (unavailable)";
            }
        }

        const names = Object.keys(available).filter(name => available[name]);
        console.info("audio sources available: " + names.join(", "));
        this.#elements.sourceSelect.dataset.available = names.join(",");
    }

    async #start() {
        if (this.#started)
            return;

        this.#started = true;
        this.#elements.overlay.hidden = true;
        this.#elements.toolbar.hidden = false;
        await this.#selectSource(this.#elements.sourceSelect.value);
    }

    #selectSource(source) {
        if (source === "file") {
            this.#elements.audioFileInput.click();
            return this.#switching;
        }

        this.#switching = this.#switching.then(() => this.#activateSource(source));
        return this.#switching;
    }

    #playAudioFile(input) {
        const file = input.files?.[0];
        input.value = "";

        if (file === undefined) {
            this.#elements.sourceSelect.value = this.#source;
            return;
        }

        const player = this.#elements.audioPlayer;
        URL.revokeObjectURL(player.src);
        player.src = URL.createObjectURL(file);
        player.dataset.name = file.name;
        this.#switching = this.#switching.then(() => this.#activateSource("file"));
    }

    async #activateSource(source) {
        this.#stopSource();

        try {
            switch (source) {
                case "wav":
                    if (this.#wavPath === null)
                        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "no deterministic WAV in " + MANIFEST_URL);
                    this.#wavFeeder = new WavFeeder(this.#module, this.#wavPath);
                    break;
                case "microphone":
                    this.#audioInput = await AudioInput.fromMicrophone(this.#view);
                    break;
                case "file":
                    await this.#elements.audioPlayer.play();
                    this.#audioInput = await AudioInput.fromMediaElement(this.#view, this.#elements.audioPlayer);
                    break;
                case "tab":
                    this.#audioInput = await AudioInput.fromDisplayCapture(this.#view);
                    if (this.#audioInput === null)
                        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "tab audio capture is not supported in this browser");
                    break;
                default:
                    throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "unknown audio source " + source);
            }

            this.#source = source;
            this.hideError();
        } catch (error) {
            this.#stopSource();
            this.#source = "none";
            this.showError(error);
        }

        this.#elements.sourceSelect.value = this.#source === "none" ? source : this.#source;
        this.#updateStatus();
    }

    #stopSource() {
        this.#audioInput?.stop();
        this.#audioInput = null;
        this.#wavFeeder?.close();
        this.#wavFeeder = null;

        if (!this.#elements.audioPlayer.paused)
            this.#elements.audioPlayer.pause();
    }

    #stepPreset(step) {
        if (this.#presets.length === 0)
            return;

        const count = this.#presets.length;
        this.#loadPreset((this.#presetIndex + step + count) % count, PRESET_BLEND_SECONDS);
    }

    #loadPreset(index, blendSeconds) {
        if (this.#view === null)
            return;

        const path = this.#presets[index];

        try {
            this.#view.loadPreset(path, blendSeconds);
            this.#presetIndex = index;
            this.#presetName = presetName(path);
            this.#elements.presetSelect.value = String(index);
            this.#reportLastError(this.#presetName);
            console.info("preset loaded: " + this.#presetName);
        } catch (error) {
            this.showError(error);
        }

        this.#updateStatus();
    }

    async #loadMilkFile(input) {
        const file = input.files?.[0];
        input.value = "";

        if (file === undefined || this.#view === null)
            return;

        try {
            const text = await file.text();

            if (!PRESET_SECTION.test(text) || !PRESET_KEY.test(text))
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_PARSE, file.name + " is not a MilkDrop preset (needs a [preset00] section and preset keys)");

            this.#view.loadPresetText(text, PRESET_BLEND_SECONDS);
            this.#presetName = file.name.replace(/\.milk$/i, "");
            this.#showCustomPreset(this.#presetName);
            this.#reportLastError(file.name);
            console.info("preset text loaded: " + file.name);
        } catch (error) {
            this.showError(error);
        }

        this.#updateStatus();
    }

    #showCustomPreset(name) {
        const select = this.#elements.presetSelect;
        let option = select.querySelector("option[value=\"" + CUSTOM_PRESET_VALUE + "\"]");

        if (option === null) {
            option = new Option("", CUSTOM_PRESET_VALUE);
            option.disabled = true;
            select.append(option);
        }

        option.textContent = "Loaded: " + name;
        select.value = CUSTOM_PRESET_VALUE;
    }

    #reportLastError(label) {
        const text = this.#view.context?.lastError() ?? "";

        if (text !== "")
            this.showError(new SynesthesiaError(SynesthesiaError.SV_ERR_PARSE, label + ": " + text));
        else
            this.hideError();
    }

    #handleKey(event) {
        if (event.target instanceof HTMLSelectElement || event.target instanceof HTMLInputElement)
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
            case "f":
            case "F":
                this.#toggleFullscreen();
                break;
            default:
                break;
        }
    }

    #toggleFullscreen() {
        const request = document.fullscreenElement !== null
            ? document.exitFullscreen()
            : document.documentElement.requestFullscreen();
        request.catch(error => this.showError(new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "fullscreen unavailable: " + error.message)));
    }

    #updateStatus() {
        const source = this.#source === "file"
            ? "file " + (this.#elements.audioPlayer.dataset.name ?? "")
            : this.#source;
        this.#elements.statusLine.textContent = "Preset: " + this.#presetName + "  |  Audio: " + source
            + "  |  Available: " + (this.#elements.sourceSelect.dataset.available ?? "");
    }

    #updateMeters() {
        const context = this.#view?.context ?? null;

        if (context !== null && !context.isClosed()) {
            const levels = context.audioLevels();
            setMeter(this.#elements.meters.bass, levels.bass);
            setMeter(this.#elements.meters.mid, levels.mid);
            setMeter(this.#elements.meters.treble, levels.treble);
        }

        requestAnimationFrame(() => this.#updateMeters());
    }
}

function presetName(path) {
    return path.split("/").pop().replace(/\.milk$/i, "");
}

function setMeter({ bar, value }, level) {
    const safe = Number.isFinite(level) ? Math.max(level, 0) : 0;
    bar.style.width = (Math.min(safe / METER_FULL_SCALE, 1) * 100).toFixed(1) + "%";
    value.textContent = safe.toFixed(2);
}

function elementsById() {
    const byId = id => document.getElementById(id);
    return {
        canvas: byId("visualizer"),
        errorBanner: byId("error-banner"),
        errorText: byId("error-text"),
        errorDismiss: byId("error-dismiss"),
        overlay: byId("overlay"),
        loadStatus: byId("load-status"),
        loadProgress: byId("load-progress"),
        startButton: byId("start-button"),
        toolbar: byId("toolbar"),
        previousPreset: byId("previous-preset"),
        nextPreset: byId("next-preset"),
        presetSelect: byId("preset-select"),
        loadMilk: byId("load-milk"),
        milkInput: byId("milk-input"),
        sourceSelect: byId("source-select"),
        audioFileInput: byId("audio-input"),
        audioPlayer: byId("audio-player"),
        fullscreenButton: byId("fullscreen-button"),
        statusLine: byId("status-line"),
        capturePanel: byId("capture-panel"),
        captureStatus: byId("capture-status"),
        captureFiles: byId("capture-files"),
        meters: {
            bass: { bar: byId("meter-bass"), value: byId("value-bass") },
            mid: { bar: byId("meter-mid"), value: byId("value-mid") },
            treble: { bar: byId("meter-treble"), value: byId("value-treble") }
        }
    };
}

const elements = elementsById();
const parameters = new URLSearchParams(location.search);

if (parameters.has("capture")) {
    elements.overlay.hidden = true;
    elements.capturePanel.hidden = false;
    runCapture(parameters, elements);
} else {
    const app = new SampleApp(elements);
    window.addEventListener("error", event => app.showError(event.error ?? event.message));
    window.addEventListener("unhandledrejection", event => app.showError(event.reason));
    app.run();
}
