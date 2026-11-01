//
//  wav-feeder.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { ResourceLoader, fetchBytes } from "./resource-loader.js";
import { SynesthesiaError } from "./synesthesia-error.js";

const INFO_FIELDS = 3;
const INT_BYTES = 4;
const SAMPLE_BYTES = 2;

export class WavFeeder {

    static FRAMES_PER_RENDER_FRAME = 735;

    #module;
    #path;
    #handle;
    #channels;
    #sampleRate;
    #frameCount;
    #buffer = 0;
    #bufferFrames = 0;
    #frameRemainder = 0;
    #framesFed = 0;

    constructor(module, path) {
        const pointer = module.stringToNewUTF8(path);
        let opened;

        try {
            opened = module._sv_web_wav_open(pointer);
        } finally {
            module._free(pointer);
        }

        if (opened === 0)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "could not load 16-bit PCM WAV " + path);

        const info = module._malloc(INFO_FIELDS * INT_BYTES);

        try {
            module._sv_web_wav_info(opened, info);
            const index = info / INT_BYTES;
            this.#channels = module.HEAP32[index];
            this.#sampleRate = module.HEAP32[index + 1];
            this.#frameCount = module.HEAP32[index + 2];
        } finally {
            module._free(info);
        }

        this.#module = module;
        this.#path = path;
        this.#handle = opened;
        console.info("WavFeeder open " + path + ": " + this.#frameCount + " frames, " + this.#channels + " ch, " + this.#sampleRate + " Hz");
    }

    static async fromUrl(module, url) {
        const location = new URL(url, document.baseURI);
        const data = await fetchBytes(location);
        const name = decodeURIComponent(location.pathname.split("/").pop() || "audio.wav");
        const path = new ResourceLoader(module).writeFile("audio", name, data);
        return new WavFeeder(module, path);
    }

    get path() {
        return this.#path;
    }

    get channels() {
        return this.#channels;
    }

    get sampleRate() {
        return this.#sampleRate;
    }

    get frameCount() {
        return this.#frameCount;
    }

    get framesFed() {
        return this.#framesFed;
    }

    feedNextRenderFrame(context) {
        return this.feedFrames(context, WavFeeder.FRAMES_PER_RENDER_FRAME);
    }

    feed(context, deltaSeconds) {
        if (deltaSeconds <= 0 || this.#sampleRate <= 0)
            return 0;

        const frames = this.#frameRemainder + deltaSeconds * this.#sampleRate;
        const wholeFrames = Math.floor(frames);
        this.#frameRemainder = frames - wholeFrames;
        return this.feedFrames(context, Math.min(wholeFrames, this.#frameCount));
    }

    feedFrames(context, frames) {
        if (this.#handle === 0)
            throw new Error("WavFeeder is closed");

        if (frames <= 0)
            return 0;

        if (this.#bufferFrames < frames) {
            if (this.#buffer !== 0)
                this.#module._free(this.#buffer);

            this.#buffer = this.#module._malloc(frames * this.#channels * SAMPLE_BYTES);
            this.#bufferFrames = frames;
        }

        const read = this.#module._sv_web_wav_read(this.#handle, frames, this.#buffer);

        if (read <= 0)
            return 0;

        const start = this.#buffer / SAMPLE_BYTES;
        const samples = this.#module.HEAP16.slice(start, start + read * this.#channels);
        context.pushPcm(samples, read, this.#channels);
        this.#framesFed += read;
        return read;
    }

    reset() {
        this.#frameRemainder = 0;
    }

    close() {
        if (this.#handle === 0)
            return;

        this.#module._sv_web_wav_close(this.#handle);
        this.#handle = 0;

        if (this.#buffer !== 0)
            this.#module._free(this.#buffer);

        this.#buffer = 0;
        this.#bufferFrames = 0;
        console.info("WavFeeder close " + this.#path + ": fed " + this.#framesFed + " frames");
    }
}
