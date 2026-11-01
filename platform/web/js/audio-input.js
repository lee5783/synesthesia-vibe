//
//  audio-input.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { SynesthesiaError } from "./synesthesia-error.js";

const PROCESSOR_NAME = "synesthesia-audio-input";
const WORKLET_URL = new URL("./audio-input-worklet.js", import.meta.url);
const BLOCK_FRAMES = 512;
const PREFERRED = "preferred";
const NATIVE = "native";

const audioContexts = new Map();
const workletLoads = new WeakMap();
const mediaElementSources = new WeakMap();

export class AudioInput {

    static SAMPLE_RATE = 44100;
    static CHANNELS = 2;

    #target;
    #label;
    #audioContext = null;
    #source = null;
    #node = null;
    #sink = null;
    #stream = null;
    #running = false;
    #framesPushed = 0;

    constructor(target, label) {
        this.#target = target;
        this.#label = label;
    }

    static isMicrophoneSupported() {
        return hasAudioWorklet() && globalThis.isSecureContext === true && typeof navigator.mediaDevices?.getUserMedia === "function";
    }

    static isMediaElementSupported() {
        return hasAudioWorklet();
    }

    static isDisplayCaptureSupported() {
        return hasAudioWorklet() && typeof navigator.mediaDevices?.getDisplayMedia === "function" && isChromium();
    }

    static async fromMicrophone(target, constraints = {}) {
        if (!AudioInput.isMicrophoneSupported())
            throw notSupported("microphone capture needs getUserMedia, an AudioWorklet and a secure context");

        const preferred = preferredAudioContext();
        let stream;

        try {
            stream = await navigator.mediaDevices.getUserMedia({
                audio: {
                    channelCount: AudioInput.CHANNELS,
                    echoCancellation: false,
                    noiseSuppression: false,
                    autoGainControl: false,
                    ...constraints
                }
            });
        } catch (error) {
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "microphone unavailable: " + error.message);
        }

        const input = new AudioInput(target, "microphone");
        await input.#startStream(stream, preferred);
        return input;
    }

    static async fromMediaElement(target, element) {
        if (!AudioInput.isMediaElementSupported())
            throw notSupported("media element capture needs an AudioWorklet");

        let attached = mediaElementSources.get(element);

        if (attached === undefined) {
            const audioContext = preferredAudioContext();
            const source = audioContext.createMediaElementSource(element);
            source.connect(audioContext.destination);
            attached = { audioContext, source };
            mediaElementSources.set(element, attached);
        } else {
            attached.audioContext.resume();
        }

        const input = new AudioInput(target, "media-element");
        await input.#start(attached.audioContext, attached.source, null);
        return input;
    }

    static async fromDisplayCapture(target) {
        if (!AudioInput.isDisplayCaptureSupported())
            return null;

        const preferred = preferredAudioContext();
        let stream;

        try {
            stream = await navigator.mediaDevices.getDisplayMedia({ video: true, audio: true, systemAudio: "include" });
        } catch (error) {
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "display capture unavailable: " + error.message);
        }

        if (stream.getAudioTracks().length === 0) {
            stream.getTracks().forEach(track => track.stop());
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "the shared surface carries no audio; share a tab with its audio enabled");
        }

        const input = new AudioInput(target, "display-capture");
        await input.#startStream(stream, preferred);
        return input;
    }

    get isRunning() {
        return this.#running;
    }

    get framesPushed() {
        return this.#framesPushed;
    }

    get inputSampleRate() {
        return this.#audioContext?.sampleRate ?? 0;
    }

    stop() {
        if (!this.#running)
            return;

        this.#running = false;
        this.#node.port.postMessage("stop");
        this.#node.port.onmessage = null;
        this.#source.disconnect(this.#node);
        this.#node.disconnect();
        this.#sink.disconnect();

        if (this.#stream !== null) {
            this.#source.disconnect();
            this.#stream.getTracks().forEach(track => track.stop());
        }

        this.#node = null;
        this.#sink = null;
        this.#source = null;
        this.#stream = null;
        console.info("AudioInput stop (" + this.#label + "): pushed " + this.#framesPushed + " frames");
    }

    async #startStream(stream, preferred) {
        let audioContext = preferred;
        let source;

        try {
            source = audioContext.createMediaStreamSource(stream);
        } catch (error) {
            if (error.name !== "NotSupportedError") {
                stream.getTracks().forEach(track => track.stop());
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, this.#label + " could not be connected: " + error.message);
            }

            audioContext = nativeAudioContext();
            source = audioContext.createMediaStreamSource(stream);
        }

        stream.getAudioTracks().forEach(track => track.addEventListener("ended", () => this.stop()));

        try {
            await this.#start(audioContext, source, stream);
        } catch (error) {
            stream.getTracks().forEach(track => track.stop());
            throw error;
        }
    }

    async #start(audioContext, source, stream) {
        await loadWorklet(audioContext);
        const node = new AudioWorkletNode(audioContext, PROCESSOR_NAME, {
            numberOfInputs: 1,
            numberOfOutputs: 1,
            outputChannelCount: [1],
            channelCount: AudioInput.CHANNELS,
            channelCountMode: "explicit",
            channelInterpretation: "speakers",
            processorOptions: { targetRate: AudioInput.SAMPLE_RATE, blockFrames: BLOCK_FRAMES }
        });
        const sink = audioContext.createGain();
        sink.gain.value = 0;
        node.port.onmessage = event => this.#push(event.data);
        source.connect(node);
        node.connect(sink);
        sink.connect(audioContext.destination);

        this.#audioContext = audioContext;
        this.#source = source;
        this.#node = node;
        this.#sink = sink;
        this.#stream = stream;
        this.#framesPushed = 0;
        this.#running = true;
        await audioContext.resume();

        const resampled = audioContext.sampleRate !== AudioInput.SAMPLE_RATE ? " (linear resample in the worklet)" : "";
        console.info("AudioInput start (" + this.#label + "): " + audioContext.sampleRate + " Hz -> " + AudioInput.SAMPLE_RATE + " Hz"
            + resampled + ", " + AudioInput.CHANNELS + " ch, " + audioContext.state);
    }

    #push(block) {
        const context = resolveTarget(this.#target);

        if (context === null || context === undefined || context.isClosed())
            return;

        const frames = block.length / AudioInput.CHANNELS;
        context.pushPcm(block, frames, AudioInput.CHANNELS);
        this.#framesPushed += frames;
    }
}

function resolveTarget(target) {
    if (typeof target === "function")
        return target();

    if (target !== null && typeof target?.pushPcm !== "function" && "context" in target)
        return target.context;

    return target;
}

function preferredAudioContext() {
    let audioContext = audioContexts.get(PREFERRED);

    if (audioContext === undefined) {
        try {
            audioContext = new AudioContext({ sampleRate: AudioInput.SAMPLE_RATE, latencyHint: "interactive" });
        } catch {
            audioContext = nativeAudioContext();
        }
        audioContexts.set(PREFERRED, audioContext);
    }

    audioContext.resume();
    return audioContext;
}

function nativeAudioContext() {
    let audioContext = audioContexts.get(NATIVE);

    if (audioContext === undefined) {
        audioContext = new AudioContext({ latencyHint: "interactive" });
        audioContexts.set(NATIVE, audioContext);
    }

    audioContext.resume();
    return audioContext;
}

function loadWorklet(audioContext) {
    let load = workletLoads.get(audioContext);

    if (load === undefined) {
        load = audioContext.audioWorklet.addModule(WORKLET_URL.href);
        workletLoads.set(audioContext, load);
    }

    return load;
}

function hasAudioWorklet() {
    return typeof AudioContext === "function" && typeof AudioWorkletNode === "function";
}

function isChromium() {
    const brands = navigator.userAgentData?.brands ?? [];
    return brands.some(brand => brand.brand === "Chromium");
}

function notSupported(message) {
    const error = new SynesthesiaError(SynesthesiaError.SV_ERR_IO, message);
    error.name = "NotSupportedError";
    return error;
}
