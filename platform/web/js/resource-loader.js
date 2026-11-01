//
//  resource-loader.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { SynesthesiaError } from "./synesthesia-error.js";

const CATEGORIES = Object.freeze(["presets", "textures", "audio"]);

export class ResourceLoader {

    static ROOT_DIRECTORY = "/synesthesiavibe";
    static PRESETS_DIRECTORY = "/synesthesiavibe/presets";
    static TEXTURES_DIRECTORY = "/synesthesiavibe/textures";
    static AUDIO_DIRECTORY = "/synesthesiavibe/audio";
    static CACHE_DIRECTORY = "/synesthesiavibe/cache";
    static PRESET_EXTENSION = ".milk";

    #module;

    constructor(module) {
        this.#module = module;
    }

    static configuration() {
        return { resourcePath: ResourceLoader.ROOT_DIRECTORY, cachePath: ResourceLoader.CACHE_DIRECTORY };
    }

    async load(manifestUrl, onProgress = null) {
        const manifestLocation = new URL(manifestUrl, document.baseURI);
        const manifest = await fetchJson(manifestLocation);
        const entries = [];

        for (const category of CATEGORIES) {
            const names = manifest[category] ?? [];

            if (!Array.isArray(names))
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_PARSE, manifestLocation.href + ": \"" + category + "\" is not an array");

            for (const name of names)
                entries.push({ category, name: String(name) });
        }

        this.ensureDirectories();
        const started = performance.now();
        const total = entries.length;
        let loaded = 0;
        let bytes = 0;

        const written = await Promise.all(entries.map(async entry => {
            const url = new URL(encodePath(entry.name), manifestLocation);
            const data = await fetchBytes(url);
            const path = ResourceLoader.ROOT_DIRECTORY + "/" + entry.category + "/" + baseName(entry.name);
            this.#module.FS.writeFile(path, data);
            loaded += 1;
            bytes += data.length;

            if (onProgress !== null)
                onProgress({ loaded, total, bytes, path });

            return { category: entry.category, path };
        }));

        const result = { presets: [], textures: [], audio: [], bytes };

        for (const file of written)
            result[file.category].push(file.path);

        CATEGORIES.forEach(category => result[category].sort());
        console.info("ResourceLoader wrote " + result.presets.length + " presets, " + result.textures.length + " textures, "
            + result.audio.length + " audio files (" + bytes + " bytes) in " + Math.round(performance.now() - started) + " ms");
        return result;
    }

    ensureDirectories() {
        const fs = this.#module.FS;

        for (const directory of [ResourceLoader.PRESETS_DIRECTORY, ResourceLoader.TEXTURES_DIRECTORY,
            ResourceLoader.AUDIO_DIRECTORY, ResourceLoader.CACHE_DIRECTORY]) {
            if (!exists(fs, directory))
                fs.mkdirTree(directory);
        }
    }

    writeFile(category, name, data) {
        if (!CATEGORIES.includes(category))
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "unknown resource category " + category);

        this.ensureDirectories();
        const path = ResourceLoader.ROOT_DIRECTORY + "/" + category + "/" + baseName(name);
        this.#module.FS.writeFile(path, data);
        return path;
    }

    presets() {
        const fs = this.#module.FS;

        if (!exists(fs, ResourceLoader.PRESETS_DIRECTORY))
            return [];

        return fs.readdir(ResourceLoader.PRESETS_DIRECTORY)
            .filter(name => name.toLowerCase().endsWith(ResourceLoader.PRESET_EXTENSION))
            .sort()
            .map(name => ResourceLoader.PRESETS_DIRECTORY + "/" + name);
    }
}

function exists(fs, path) {
    try {
        fs.stat(path);
        return true;
    } catch {
        return false;
    }
}

function baseName(name) {
    const base = name.split("/").pop();

    if (base === "" || base === "." || base === "..")
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "invalid resource name " + name);

    return base;
}

function encodePath(name) {
    return name.split("/").map(encodeURIComponent).join("/");
}

async function fetchResponse(url) {
    let response;

    try {
        response = await fetch(url);
    } catch (error) {
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, url.href + ": " + error.message);
    }

    if (!response.ok)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, url.href + ": HTTP " + response.status);

    return response;
}

async function fetchJson(url) {
    const response = await fetchResponse(url);

    try {
        return await response.json();
    } catch (error) {
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_PARSE, url.href + ": " + error.message);
    }
}

export async function fetchBytes(url) {
    const response = await fetchResponse(url);
    return new Uint8Array(await response.arrayBuffer());
}
