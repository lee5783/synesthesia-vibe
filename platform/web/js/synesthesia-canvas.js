//
//  synesthesia-canvas.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { ResourceLoader } from "./resource-loader.js";
import { SynesthesiaContext } from "./synesthesia-context.js";
import { SynesthesiaError } from "./synesthesia-error.js";
import { createWebGl2Context, makeWebGlContextCurrent, registerWebGlContext, releaseWebGlContext } from "./webgl-context.js";

const MAX_DEVICE_PIXEL_RATIO = 2;
const DEFAULT_FRAME_SECONDS = 1 / 60;
const MAX_FRAME_SECONDS = 0.25;
const MS_PER_SECOND = 1000;

export class SynesthesiaCanvas {

    #canvas;
    #module;
    #configuration;
    #onError;
    #onContextCreated;
    #onBeforeRender;
    #gl;
    #glHandle = 0;
    #context = null;
    #preset = null;
    #frameRequest = 0;
    #lastFrameMs = 0;
    #paused = false;
    #hidden = false;
    #lost = false;
    #closed = false;
    #resizeObserver = null;
    #listeners = [];

    constructor(canvas, module, options = {}) {
        this.#canvas = canvas;
        this.#module = module;
        this.#configuration = { ...ResourceLoader.configuration(), ...(options.configuration ?? {}) };
        this.#onError = options.onError ?? null;
        this.#onContextCreated = options.onContextCreated ?? null;
        this.#onBeforeRender = options.onBeforeRender ?? null;
        this.#hidden = typeof document !== "undefined" && document.visibilityState === "hidden";
    }

    static async create(canvas, module, options = {}) {
        const view = new SynesthesiaCanvas(canvas, module, options);

        try {
            view.#gl = createWebGl2Context(canvas);
            view.#updateBackingSize();
            await view.#attach();
        } catch (error) {
            view.close();
            throw error;
        }

        view.#listen();
        view.#startLoop();
        return view;
    }

    get canvas() {
        return this.#canvas;
    }

    get module() {
        return this.#module;
    }

    get context() {
        return this.#context;
    }

    get isPaused() {
        return this.#paused;
    }

    get isContextLost() {
        return this.#lost;
    }

    glStrings() {
        return SynesthesiaContext.glStrings(this.#module);
    }

    loadPreset(path, blendSeconds = 0) {
        this.#requireContext().loadPreset(path, blendSeconds);
        this.#preset = { path, text: null };
    }

    loadPresetText(text, blendSeconds = 0) {
        this.#requireContext().loadPresetText(text, blendSeconds);
        this.#preset = { path: null, text };
    }

    pause() {
        this.#paused = true;
        this.#applyRunState();
    }

    resume() {
        this.#paused = false;
        this.#applyRunState();
    }

    close() {
        if (this.#closed)
            return;

        this.#closed = true;
        this.#stopLoop();
        this.#resizeObserver?.disconnect();
        this.#resizeObserver = null;
        this.#listeners.forEach(([target, type, handler]) => target.removeEventListener(type, handler));
        this.#listeners = [];

        if (this.#context !== null) {
            makeWebGlContextCurrent(this.#module, this.#glHandle);
            this.#context.close();
            this.#context = null;
        }

        releaseWebGlContext(this.#module, this.#glHandle);
        this.#glHandle = 0;
    }

    async #attach() {
        this.#glHandle = registerWebGlContext(this.#module, this.#gl);
        SynesthesiaContext.initGl(this.#module);
        const context = await SynesthesiaContext.create(this.#module, {
            ...this.#configuration,
            width: this.#canvas.width,
            height: this.#canvas.height
        });

        if (this.#paused || this.#hidden)
            context.pause();

        this.#context = context;
        this.#lastFrameMs = 0;

        if (this.#onContextCreated !== null)
            this.#onContextCreated(context);
    }

    #listen() {
        this.#addListener(this.#canvas, "webglcontextlost", event => this.#handleContextLost(event));
        this.#addListener(this.#canvas, "webglcontextrestored", () => this.#handleContextRestored());

        if (typeof document !== "undefined")
            this.#addListener(document, "visibilitychange", () => this.#handleVisibilityChange());

        if (typeof ResizeObserver !== "undefined" && this.#canvas instanceof HTMLCanvasElement) {
            this.#resizeObserver = new ResizeObserver(() => this.#handleResize());
            this.#resizeObserver.observe(this.#canvas);
        }
    }

    #addListener(target, type, handler) {
        target.addEventListener(type, handler);
        this.#listeners.push([target, type, handler]);
    }

    #handleContextLost(event) {
        event.preventDefault();
        this.#lost = true;
        this.#stopLoop();
        const lost = this.#context;
        this.#context = null;

        if (lost !== null) {
            makeWebGlContextCurrent(this.#module, this.#glHandle);
            lost.closeAfterContextLoss();
        }

        releaseWebGlContext(this.#module, this.#glHandle);
        this.#glHandle = 0;
    }

    async #handleContextRestored() {
        if (this.#closed)
            return;

        try {
            this.#updateBackingSize();
            await this.#attach();
            this.#lost = false;
            this.#reloadPreset();
            this.#startLoop();
        } catch (error) {
            this.#report(error);
        }
    }

    #reloadPreset() {
        if (this.#preset === null)
            return;

        if (this.#preset.path !== null)
            this.#context.loadPreset(this.#preset.path, 0);
        else
            this.#context.loadPresetText(this.#preset.text, 0);
    }

    #handleVisibilityChange() {
        this.#hidden = document.visibilityState === "hidden";
        this.#applyRunState();
    }

    #applyRunState() {
        if (this.#context === null)
            return;

        makeWebGlContextCurrent(this.#module, this.#glHandle);

        if (this.#paused || this.#hidden) {
            this.#context.pause();
            this.#stopLoop();
        } else {
            this.#context.resume();
            this.#startLoop();
        }
    }

    #handleResize() {
        if (!this.#updateBackingSize() || this.#context === null)
            return;

        makeWebGlContextCurrent(this.#module, this.#glHandle);
        this.#context.resize(this.#canvas.width, this.#canvas.height);
    }

    #updateBackingSize() {
        const ratio = Math.min(globalThis.devicePixelRatio || 1, MAX_DEVICE_PIXEL_RATIO);
        const cssWidth = this.#canvas.clientWidth ?? 0;
        const cssHeight = this.#canvas.clientHeight ?? 0;
        const width = Math.max(1, Math.round((cssWidth || this.#canvas.width) * (cssWidth ? ratio : 1)));
        const height = Math.max(1, Math.round((cssHeight || this.#canvas.height) * (cssHeight ? ratio : 1)));

        if (width === this.#canvas.width && height === this.#canvas.height)
            return false;

        this.#canvas.width = width;
        this.#canvas.height = height;
        return true;
    }

    #startLoop() {
        if (this.#frameRequest !== 0 || this.#closed || this.#lost || this.#paused || this.#hidden || this.#context === null)
            return;

        this.#lastFrameMs = 0;
        this.#frameRequest = requestAnimationFrame(now => this.#frame(now));
    }

    #stopLoop() {
        if (this.#frameRequest !== 0)
            cancelAnimationFrame(this.#frameRequest);

        this.#frameRequest = 0;
    }

    #frame(now) {
        this.#frameRequest = 0;
        const context = this.#context;

        if (context === null)
            return;

        const deltaSeconds = this.#lastFrameMs !== 0
            ? Math.min((now - this.#lastFrameMs) / MS_PER_SECOND, MAX_FRAME_SECONDS)
            : this.#firstFrameSeconds();
        this.#lastFrameMs = now;

        try {
            makeWebGlContextCurrent(this.#module, this.#glHandle);

            if (this.#onBeforeRender !== null)
                this.#onBeforeRender(context, deltaSeconds);

            context.render(deltaSeconds, 0);
        } catch (error) {
            this.#report(error);
        }

        this.#frameRequest = requestAnimationFrame(next => this.#frame(next));
    }

    #firstFrameSeconds() {
        const fps = this.#configuration.fpsHint ?? 0;
        return fps > 0 ? 1 / fps : DEFAULT_FRAME_SECONDS;
    }

    #requireContext() {
        if (this.#context === null)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_GL, this.#lost ? "the WebGL context is lost" : "SynesthesiaCanvas has no context");

        makeWebGlContextCurrent(this.#module, this.#glHandle);
        return this.#context;
    }

    #report(error) {
        const reported = error instanceof SynesthesiaError
            ? error
            : new SynesthesiaError(SynesthesiaError.SV_ERR_GL, (error?.message ?? String(error)) + this.#lastErrorSuffix());

        if (this.#onError !== null)
            this.#onError(reported);
        else
            console.error(reported);
    }

    #lastErrorSuffix() {
        if (this.#context === null || this.#context.isClosed())
            return "";

        const text = this.#context.lastError();
        return text !== "" ? " (" + text + ")" : "";
    }
}
