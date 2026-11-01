//
//  capture-session.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { ResourceLoader } from "./resource-loader.js";
import { SynesthesiaContext } from "./synesthesia-context.js";
import { SynesthesiaError } from "./synesthesia-error.js";
import { createWebGl2Context, registerWebGlContext, releaseWebGlContext } from "./webgl-context.js";

export class CaptureSession {

    static GL_STRINGS_FILE_NAME = "gl-strings.txt";
    static CAPTURE_WIDTH = 838;
    static CAPTURE_HEIGHT = 441;
    static CAPTURE_RNG_SEED = 20260927;
    static CAPTURE_MESH_X = 64;
    static CAPTURE_MESH_Y = 48;
    static CAPTURE_TEXSIZE_X = 848;
    static CAPTURE_TEXSIZE_Y = 448;
    static CAPTURE_FPS = 60;
    static PINNED_DATE = Object.freeze([2000, 1, 1, 6, 0, 0, 0, 0]);

    #module;
    #canvas;
    #outputDirectory = null;

    constructor(module, canvas) {
        this.#module = module;
        this.#canvas = canvas;
    }

    static standardConfiguration(configuration = {}) {
        return {
            resourcePath: configuration.resourcePath ?? ResourceLoader.ROOT_DIRECTORY,
            textureSearchPath: configuration.textureSearchPath ?? null,
            cachePath: configuration.cachePath ?? ResourceLoader.CACHE_DIRECTORY,
            width: CaptureSession.CAPTURE_WIDTH,
            height: CaptureSession.CAPTURE_HEIGHT,
            meshX: CaptureSession.CAPTURE_MESH_X,
            meshY: CaptureSession.CAPTURE_MESH_Y,
            texsizeX: CaptureSession.CAPTURE_TEXSIZE_X,
            texsizeY: CaptureSession.CAPTURE_TEXSIZE_Y,
            fpsHint: CaptureSession.CAPTURE_FPS,
            rngSeed: CaptureSession.CAPTURE_RNG_SEED,
            deterministic: true,
            pinnedDate: configuration.pinnedDate ?? CaptureSession.PINNED_DATE,
            pinnedMouse: configuration.pinnedMouse ?? null
        };
    }

    static fileNameForFrame(frame, extension) {
        return "frame_" + String(frame).padStart(4, "0") + "." + extension;
    }

    async run({ preset, wav = null, frames, outDir, writesFrameState = true, configuration = {} }) {
        const wanted = [...new Set(frames.filter(frame => Number.isInteger(frame) && frame >= 0))].sort((a, b) => a - b);

        if (wanted.length === 0)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "capture frame list is empty");

        const fs = this.#module.FS;
        new ResourceLoader(this.#module).ensureDirectories();
        fs.mkdirTree(outDir);
        this.#outputDirectory = outDir;

        const gl = createWebGl2Context(this.#canvas);
        const handle = registerWebGlContext(this.#module, gl);

        try {
            SynesthesiaContext.initGl(this.#module);
            const context = await SynesthesiaContext.create(this.#module, CaptureSession.standardConfiguration(configuration));

            try {
                context.loadPreset(preset, 0);
                fs.writeFile(outDir + "/" + CaptureSession.GL_STRINGS_FILE_NAME,
                    SynesthesiaContext.glStrings(this.#module) + CaptureSession.#unmaskedGlStrings(gl) + "USER_AGENT=" + navigator.userAgent + "\n");
                context.captureFrames(wav, wanted, outDir, writesFrameState);
            } finally {
                context.close();
            }
        } finally {
            releaseWebGlContext(this.#module, handle);
        }

        return wanted.flatMap(frame => writesFrameState
            ? [CaptureSession.fileNameForFrame(frame, "png"), CaptureSession.fileNameForFrame(frame, "json")]
            : [CaptureSession.fileNameForFrame(frame, "png")]);
    }

    static #unmaskedGlStrings(gl) {
        const extension = gl.getExtension("WEBGL_debug_renderer_info");

        if (extension === null)
            return "";

        return [["UNMASKED_VENDOR", extension.UNMASKED_VENDOR_WEBGL], ["UNMASKED_RENDERER", extension.UNMASKED_RENDERER_WEBGL]]
            .map(([key, parameter]) => [key, gl.getParameter(parameter)])
            .filter(([, value]) => typeof value === "string" && value.length > 0)
            .map(([key, value]) => key + "=" + value + "\n")
            .join("");
    }

    files() {
        if (this.#outputDirectory === null)
            return [];

        const fs = this.#module.FS;
        return fs.readdir(this.#outputDirectory)
            .filter(name => name !== "." && name !== "..")
            .sort()
            .map(name => ({ name, bytes: fs.readFile(this.#outputDirectory + "/" + name) }));
    }
}
