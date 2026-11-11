//
//  capture.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { CaptureSession, ResourceLoader, SynesthesiaContext, SynesthesiaError, loadSynesthesiaModule } from "synesthesia-vibe";

const CAPTURE_MANIFEST_URL = "resources/capture-manifest.json";
const CAPTURE_SET_URL = "resources/capture-set.json";
const OUTPUT_ROOT = "/synesthesiavibe/capture";
const DONE_MARKER = "done";
const NO_WAV = "none";
const DEFAULT_BLEND_SECONDS = 2.7;
const STANDARD_FRAMES = Object.freeze([0, 30, 60, 150, 300, 450, 599]);

export async function runCapture(parameters, elements) {
    const key = (parameters.get("capture") ?? "").trim();
    const out = (parameters.get("out") ?? "").trim() || key || "capture";
    const upload = (parameters.get("upload") ?? "").trim() || null;
    let line;

    try {
        if (key === "")
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "missing value for capture");

        const frames = parseFrames(parameters.get("frames"));
        report(elements, "Loading resources for " + key + "…");
        const module = await loadSynesthesiaModule();
        const loaded = await new ResourceLoader(module).load(CAPTURE_MANIFEST_URL);
        const preset = await resolvePreset(key, loaded.presets);
        const wav = resolveWav(parameters.get("wav"), loaded.audio);
        const blend = await resolveBlend(parameters, loaded.presets);

        console.info("CAPTURE START preset=" + key + " frames=" + frames.join(",") + " out=" + out
            + (blend === null ? "" : " blend=" + parameters.get("blend") + " blendAt=" + blend.atFrame + " blendSeconds=" + blend.seconds + " pattern=" + blend.pattern));
        report(elements, "Capturing " + frames.length + " frames of " + key + "…");
        const canvas = document.createElement("canvas");
        canvas.width = CaptureSession.CAPTURE_WIDTH;
        canvas.height = CaptureSession.CAPTURE_HEIGHT;
        const session = new CaptureSession(module, canvas);
        await session.run({ preset, wav, frames, outDir: OUTPUT_ROOT + "/" + out, blend });
        const files = session.files();

        for (const file of files) {
            if (upload !== null)
                await post(uploadUrl(upload, out, file.name), file.bytes);
            else
                offerDownload(elements, file);
        }

        line = "CAPTURE OK files=" + files.length + " out=" + out;
        console.info(line);
    } catch (error) {
        line = "CAPTURE FAILED " + (error instanceof Error ? error.message : String(error));
        console.error(line);
    }

    report(elements, line);
    document.title = line;

    if (upload !== null) {
        try {
            await post(uploadUrl(upload, out, DONE_MARKER), new TextEncoder().encode(line + "\n"));
        } catch (error) {
            console.error("could not post the " + DONE_MARKER + " marker: " + error.message);
            report(elements, line + " (done marker upload failed: " + error.message + ")");
        }
    }

    return line;
}

async function resolveBlend(parameters, presets) {
    const key = (parameters.get("blend") ?? "").trim();

    if (key === "")
        return null;

    const atFrame = parseNonNegative(parameters.get("blendAt"), "blendAt", 0, true);
    const seconds = parseNonNegative(parameters.get("blendSeconds"), "blendSeconds", DEFAULT_BLEND_SECONDS, false);
    const pattern = parsePattern(parameters.get("pattern"));
    return { preset: await resolvePreset(key, presets), atFrame, seconds, pattern };
}

function parseNonNegative(value, name, fallback, integer) {
    if (value === null || value.trim() === "")
        return fallback;

    const number = Number(value);

    if (!Number.isFinite(number) || number < 0 || (integer && !Number.isInteger(number)))
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "invalid " + name + " " + value);

    return number;
}

function parsePattern(value) {
    if (value === null || value.trim() === "")
        return SynesthesiaContext.BLEND_PATTERN_RANDOM;

    const number = Number(value);

    if (!Number.isInteger(number) || number < SynesthesiaContext.BLEND_PATTERN_RANDOM || number >= SynesthesiaContext.BLEND_PATTERN_NAMES.length)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "invalid pattern " + value);

    return number;
}

function parseFrames(list) {
    if (list === null || list.trim() === "")
        return [...STANDARD_FRAMES];

    const parts = list.split(",").map(part => part.trim());
    const frames = parts.filter(part => /^\d+$/.test(part)).map(Number);

    if (frames.length === 0 || frames.length !== parts.length)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "invalid frames list " + list);

    return frames.sort((a, b) => a - b);
}

async function resolvePreset(key, presets) {
    const response = await fetch(new URL(CAPTURE_SET_URL, document.baseURI));

    if (!response.ok)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, CAPTURE_SET_URL + ": HTTP " + response.status);

    const captureSet = await response.json();
    const entry = (captureSet.presets ?? []).find(candidate => candidate.id === key || candidate.file === key);
    const file = entry?.file ?? key;
    const path = presets.find(candidate => candidate.split("/").pop() === file);

    if (path === undefined)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "no bundled capture preset matches " + key);

    return path;
}

function resolveWav(value, audio) {
    if (value === NO_WAV)
        return null;

    const name = value ?? null;
    const path = audio.find(candidate => name === null ? candidate.toLowerCase().endsWith(".wav") : candidate.split("/").pop() === name);

    if (path === undefined)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, "WAV not found: " + (name ?? "deterministic WAV"));

    return path;
}

function uploadUrl(base, out, name) {
    return base.replace(/\/+$/, "") + "/" + encodeURIComponent(out) + "/" + encodeURIComponent(name);
}

async function post(url, bytes) {
    const response = await fetch(url, { method: "POST", body: bytes, headers: { "Content-Type": "application/octet-stream" } });

    if (!response.ok)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_IO, url + ": HTTP " + response.status);
}

function offerDownload(elements, file) {
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([file.bytes]));
    link.download = file.name;
    link.textContent = file.name + " (" + file.bytes.length + " bytes)";
    const item = document.createElement("li");
    item.append(link);
    elements.captureFiles.append(item);
}

function report(elements, text) {
    elements.captureStatus.textContent = text;
}
