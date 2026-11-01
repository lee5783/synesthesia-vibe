//
//  synesthesia-context.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { SynesthesiaError } from "./synesthesia-error.js";

const RENDERER_UNAVAILABLE_PREFIX = "renderer unavailable";
const PINNED_DATE_FIELDS = 8;
const AUDIO_LEVEL_FIELDS = 3;
const FLOAT_BYTES = 4;
const INT_BYTES = 4;

export class SynesthesiaContext {

    #module;
    #handle;
    #scratch = 0;
    #scratchBytes = 0;
    #levels = 0;

    constructor(module, handle) {
        this.#module = module;
        this.#handle = handle;
    }

    static initGl(module) {
        const status = module._sv_web_init_gl();

        if (status !== SynesthesiaError.SV_OK)
            throw new SynesthesiaError(status, "sv_init_gl needs a current WebGL2 context and could not load the OpenGL ES 3.0 entry points");
    }

    static glStrings(module) {
        return module.UTF8ToString(module._sv_web_gl_strings());
    }

    static async create(module, config = {}) {
        const strings = [];
        let date = 0;
        let handle = 0;

        try {
            const resourcePath = allocString(module, config.resourcePath, strings);
            const textureSearchPath = allocString(module, config.textureSearchPath, strings);
            const cachePath = allocString(module, config.cachePath, strings);
            const pinnedDate = config.pinnedDate ?? null;

            if (pinnedDate !== null) {
                if (pinnedDate.length !== PINNED_DATE_FIELDS)
                    throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "pinnedDate needs " + PINNED_DATE_FIELDS + " fields");
                date = module._malloc(PINNED_DATE_FIELDS * FLOAT_BYTES);
                module.HEAPF32.set(pinnedDate, date / FLOAT_BYTES);
            }

            const mouse = config.pinnedMouse ?? null;
            handle = module._sv_web_create(
                config.width ?? 0, config.height ?? 0,
                config.meshX ?? 0, config.meshY ?? 0,
                config.texsizeX ?? 0, config.texsizeY ?? 0,
                config.fpsHint ?? 0,
                resourcePath, textureSearchPath, cachePath,
                (config.rngSeed ?? 0) >>> 0,
                config.deterministic ? 1 : 0,
                pinnedDate !== null ? 1 : 0, date,
                mouse !== null ? 1 : 0, mouse?.x ?? 0, mouse?.y ?? 0);
        } finally {
            strings.forEach(pointer => module._free(pointer));

            if (date !== 0)
                module._free(date);
        }

        if (handle === 0)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "sv_create returned no context");

        const context = new SynesthesiaContext(module, handle);
        const error = context.lastError();

        if (error.startsWith(RENDERER_UNAVAILABLE_PREFIX)) {
            context.close();
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_GL, error);
        }

        return context;
    }

    get module() {
        return this.#module;
    }

    loadPreset(path, blendSeconds = 0) {
        const handle = this.#requireHandle();
        const pointer = this.#module.stringToNewUTF8(path);

        try {
            this.#check(this.#module._sv_web_load_preset(handle, pointer, blendSeconds));
        } finally {
            this.#module._free(pointer);
        }
    }

    loadPresetText(text, blendSeconds = 0) {
        const handle = this.#requireHandle();
        const bytes = new TextEncoder().encode(text);
        const pointer = this.#module._malloc(bytes.length + 1);

        try {
            this.#module.HEAPU8.set(bytes, pointer);
            this.#module.HEAPU8[pointer + bytes.length] = 0;
            this.#check(this.#module._sv_web_load_preset_text(handle, pointer, bytes.length, blendSeconds));
        } finally {
            this.#module._free(pointer);
        }
    }

    pushPcm(interleaved, frames = null, channels = 2) {
        const handle = this.#requireHandle();
        const isFloat = interleaved instanceof Float32Array;

        if (!isFloat && !(interleaved instanceof Int16Array))
            throw new TypeError("pushPcm takes a Float32Array or an Int16Array");

        const count = requireSamples(interleaved, frames, channels);

        if (count === 0)
            return;

        const samples = interleaved.subarray(0, count * channels);
        const pointer = this.#ensureScratch(samples.byteLength);

        if (isFloat) {
            this.#module.HEAPF32.set(samples, pointer / FLOAT_BYTES);
            this.#module._sv_web_push_pcm_float(handle, pointer, count, channels);
        } else {
            this.#module.HEAP16.set(samples, pointer / 2);
            this.#module._sv_web_push_pcm_s16(handle, pointer, count, channels);
        }
    }

    render(deltaSeconds, targetFramebuffer = 0) {
        this.#module._sv_web_render(this.#requireHandle(), deltaSeconds, targetFramebuffer >>> 0);
    }

    resize(widthPixels, heightPixels) {
        this.#module._sv_web_resize(this.#requireHandle(), widthPixels, heightPixels);
    }

    pause() {
        this.#module._sv_web_pause(this.#requireHandle());
    }

    resume() {
        this.#module._sv_web_resume(this.#requireHandle());
    }

    audioLevels() {
        const handle = this.#requireHandle();

        if (this.#levels === 0)
            this.#levels = this.#module._malloc(AUDIO_LEVEL_FIELDS * FLOAT_BYTES);

        this.#module._sv_web_get_audio_levels(handle, this.#levels);
        const index = this.#levels / FLOAT_BYTES;
        const heap = this.#module.HEAPF32;
        return { bass: heap[index], mid: heap[index + 1], treble: heap[index + 2] };
    }

    lastError() {
        return this.#module.UTF8ToString(this.#module._sv_web_last_error(this.#requireHandle()));
    }

    dumpFrameState(path) {
        const handle = this.#requireHandle();
        const pointer = this.#module.stringToNewUTF8(path);

        try {
            this.#module._sv_web_dump_frame_state(handle, pointer);
        } finally {
            this.#module._free(pointer);
        }
    }

    isClosed() {
        return this.#handle === 0;
    }

    close() {
        if (this.#handle === 0)
            return;

        this.#module._sv_web_destroy(this.#handle);
        this.#handle = 0;

        if (this.#scratch !== 0)
            this.#module._free(this.#scratch);

        if (this.#levels !== 0)
            this.#module._free(this.#levels);

        this.#scratch = 0;
        this.#scratchBytes = 0;
        this.#levels = 0;
    }

    closeAfterContextLoss() {
        this.close();
    }

    captureFrames(wavPath, frames, outputDirectory, writesFrameState) {
        const handle = this.#requireHandle();
        const strings = [];
        const list = this.#module._malloc(Math.max(frames.length, 1) * INT_BYTES);
        let status;

        try {
            this.#module.HEAP32.set(Int32Array.from(frames), list / INT_BYTES);
            const wav = allocString(this.#module, wavPath, strings);
            const directory = allocString(this.#module, outputDirectory, strings);
            status = this.#module._sv_web_capture(handle, wav, list, frames.length, directory, writesFrameState ? 1 : 0);
        } finally {
            strings.forEach(pointer => this.#module._free(pointer));
            this.#module._free(list);
        }

        if (status !== SynesthesiaError.SV_OK)
            throw new SynesthesiaError(status, this.#module.UTF8ToString(this.#module._sv_web_capture_error()));
    }

    #check(status) {
        if (status !== SynesthesiaError.SV_OK)
            throw new SynesthesiaError(status, this.lastError());
    }

    #requireHandle() {
        if (this.#handle === 0)
            throw new Error("SynesthesiaContext is closed");
        return this.#handle;
    }

    #ensureScratch(bytes) {
        if (this.#scratchBytes < bytes) {
            if (this.#scratch !== 0)
                this.#module._free(this.#scratch);

            this.#scratch = this.#module._malloc(bytes);
            this.#scratchBytes = bytes;
        }

        return this.#scratch;
    }
}

function allocString(module, text, pointers) {
    if (text === null || text === undefined)
        return 0;

    const pointer = module.stringToNewUTF8(String(text));
    pointers.push(pointer);
    return pointer;
}

function requireSamples(interleaved, frames, channels) {
    if (!Number.isInteger(channels) || channels <= 0)
        throw new RangeError("channels must be positive");

    const count = frames ?? Math.floor(interleaved.length / channels);

    if (count < 0 || count * channels > interleaved.length)
        throw new RangeError("frames * channels exceeds the buffer length");

    return count;
}
