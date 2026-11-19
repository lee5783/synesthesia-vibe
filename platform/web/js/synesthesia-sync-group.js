//
//  synesthesia-sync-group.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

import { ResourceLoader } from "./resource-loader.js";
import { SynesthesiaContext } from "./synesthesia-context.js";
import { SynesthesiaError } from "./synesthesia-error.js";
import { createWebGl2Context, makeWebGlContextCurrent, registerWebGlContext, releaseWebGlContext } from "./webgl-context.js";

const DEFAULT_FRAME_SECONDS = 1 / 60;
const MAX_FRAME_SECONDS = 0.25;
const MS_PER_SECOND = 1000;
const MIN_MAX_FPS = 1;
const DEADLINE_TOLERANCE = 0.2;
const DEFAULT_PLAYBACK_RATE = 1;
const DEFAULT_DEBUG_INTERVAL = 1;
const DEFAULT_RENDER_SCALE = 1;
const INITIAL_SIZE = 64;
const DEFAULT_BLEND_PATTERN = SynesthesiaContext.BLEND_PATTERN_RANDOM;
const SCALINGS = Object.freeze(["fill", "fit", "stretch"]);

const BLIT_VERTEX_SOURCE = `#version 300 es
out vec2 uv;
void main() {
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = vec2(corner.x, 1.0 - corner.y);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
`;

const BLIT_FRAGMENT_SOURCE = `#version 300 es
precision highp float;
uniform sampler2D source;
in vec2 uv;
out vec4 color;
void main() {
    color = vec4(texture(source, uv).rgb, 1.0);
}
`;

export class SynesthesiaSyncGroup {

    #module;
    #canvas;
    #configuration;
    #onError;
    #onBeforeRender;
    #onFramePresented;
    #gl;
    #glHandle = 0;
    #context = null;
    #members = new Map();
    #preset = null;
    #loadSerial = 0;
    #frameRequest = 0;
    #lastFrameMs = 0;
    #frameIntervalMs = 0;
    #nextDeadlineMs = 0;
    #frameCount = 0;
    #paused = false;
    #playbackRate = DEFAULT_PLAYBACK_RATE;
    #onDebugInfo = null;
    #debugInterval = null;
    #transitionConfig = null;
    #blendPattern = DEFAULT_BLEND_PATTERN;
    #onPresetRequest = null;
    #renderScale = DEFAULT_RENDER_SCALE;
    #fixedSize = null;
    #hidden = false;
    #lost = false;
    #closed = false;
    #listeners = [];

    constructor(module, options = {}) {
        this.#module = module;
        this.#configuration = { ...ResourceLoader.configuration(), ...(options.configuration ?? {}) };
        this.#onError = options.onError ?? null;
        this.#onBeforeRender = options.onBeforeRender ?? null;
        this.#onFramePresented = options.onFramePresented ?? null;
        this.#renderScale = SynesthesiaSyncGroup.#validScale(options.renderScale ?? DEFAULT_RENDER_SCALE);
        this.#fixedSize = SynesthesiaSyncGroup.#validSize(options.size ?? null);
        this.#frameIntervalMs = SynesthesiaSyncGroup.#frameIntervalFor(this.#configuration.maxFps);
        this.#hidden = typeof document !== "undefined" && document.visibilityState === "hidden";
        this.#canvas = document.createElement("canvas");
    }

    static async create(module, options = {}) {
        if (options.configuration?.deterministic)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "a SynesthesiaSyncGroup does not support a deterministic configuration");

        const group = new SynesthesiaSyncGroup(module, options);

        try {
            group.#applySize(group.#fixedSize ?? { width: INITIAL_SIZE, height: INITIAL_SIZE });
            group.#gl = createWebGl2Context(group.#canvas);
            await group.#attach();
        } catch (error) {
            group.close();
            throw error;
        }

        group.#listen();
        return group;
    }

    static get SCALINGS() {
        return SCALINGS;
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

    get memberCount() {
        return this.#members.size;
    }

    get frameCount() {
        return this.#frameCount;
    }

    get size() {
        return { width: this.#canvas.width, height: this.#canvas.height };
    }

    get playbackRate() {
        return this.#playbackRate;
    }

    setPlaybackRate(rate) {
        this.#playbackRate = rate;

        if (this.#context !== null)
            this.#context.setPlaybackRate(rate);
    }

    get onDebugInfo() {
        return this.#onDebugInfo;
    }

    set onDebugInfo(handler) {
        if (handler !== null && handler !== undefined && typeof handler !== "function")
            throw new TypeError("onDebugInfo takes a function or null");

        this.#onDebugInfo = handler ?? null;

        if (this.#context !== null)
            this.#context.onDebugInfo = this.#onDebugInfo;
    }

    get debugInterval() {
        return this.#debugInterval ?? DEFAULT_DEBUG_INTERVAL;
    }

    setDebugInterval(seconds) {
        this.#debugInterval = seconds;

        if (this.#context !== null)
            this.#context.setDebugInterval(seconds);
    }

    get transitionConfig() {
        return { ...SynesthesiaContext.DEFAULT_TRANSITION_CONFIG, ...(this.#transitionConfig ?? {}) };
    }

    setTransitionConfig(config) {
        this.#transitionConfig = { ...(this.#transitionConfig ?? {}), ...config };

        if (this.#context !== null)
            this.#context.setTransitionConfig(this.#transitionConfig);
    }

    get blendPattern() {
        return this.#blendPattern;
    }

    setBlendPattern(pattern) {
        this.#blendPattern = pattern;

        if (this.#context !== null)
            this.#context.setBlendPattern(pattern);
    }

    get onPresetRequest() {
        return this.#onPresetRequest;
    }

    set onPresetRequest(handler) {
        if (handler !== null && handler !== undefined && typeof handler !== "function")
            throw new TypeError("onPresetRequest takes a function or null");

        this.#onPresetRequest = handler ?? null;

        if (this.#context !== null)
            this.#context.onPresetRequest = this.#onPresetRequest;
    }

    get renderScale() {
        return this.#renderScale;
    }

    setRenderScale(scale) {
        this.#renderScale = SynesthesiaSyncGroup.#validScale(scale);
        this.#fitSize(true);
    }

    get fixedSize() {
        return this.#fixedSize === null ? null : { ...this.#fixedSize };
    }

    setFixedSize(size) {
        this.#fixedSize = SynesthesiaSyncGroup.#validSize(size);
        this.#fitSize();
    }

    get maxFps() {
        return this.#frameIntervalMs === 0 ? 0 : MS_PER_SECOND / this.#frameIntervalMs;
    }

    setMaxFps(maxFps) {
        this.#configuration.maxFps = maxFps;
        this.#frameIntervalMs = SynesthesiaSyncGroup.#frameIntervalFor(maxFps);
        this.#nextDeadlineMs = 0;
    }

    presetInfo() {
        return this.#requireContext().presetInfo();
    }

    glStrings() {
        return SynesthesiaContext.glStrings(this.#module);
    }

    async loadPreset(path, blendSeconds = 0) {
        const started = performance.now();
        const prepared = this.#requireContext().preparePreset(path);
        return this.#commitOnNextFrame(prepared, blendSeconds, { path, text: null }, performance.now() - started);
    }

    async loadPresetText(text, blendSeconds = 0) {
        const started = performance.now();
        const prepared = this.#requireContext().preparePresetText(text);
        return this.#commitOnNextFrame(prepared, blendSeconds, { path: null, text }, performance.now() - started);
    }

    pause() {
        this.#paused = true;
        this.#applyRunState();
    }

    resume() {
        this.#paused = false;
        this.#applyRunState();
    }

    addMember(member) {
        if (this.#closed)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "the SynesthesiaSyncGroup is closed");

        if (!SCALINGS.includes(member.scaling()))
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "syncScaling takes fill, fit or stretch");

        if (this.#members.has(member))
            return;

        this.#members.set(member, { resources: null, snapshot: null });
        this.#fitSize();
        this.#applyRunState();
    }

    removeMember(member) {
        const entry = this.#members.get(member);

        if (entry === undefined)
            return;

        this.#members.delete(member);

        if (entry.snapshot !== null)
            SynesthesiaSyncGroup.#restoreState(member.gl(), entry.snapshot);

        if (entry.resources !== null)
            SynesthesiaSyncGroup.#deleteBlitResources(member.gl(), entry.resources);

        this.#applyRunState();
    }

    invalidateMember(member) {
        const entry = this.#members.get(member);

        if (entry === undefined)
            return;

        entry.resources = null;
        entry.snapshot = null;
        this.refreshMember(member);
    }

    refreshMember(member) {
        if (!this.#members.has(member))
            return;

        this.#fitSize();
        this.#applyRunState();
    }

    close() {
        if (this.#closed)
            return;

        this.#closed = true;
        this.#stopLoop();
        this.#listeners.forEach(([target, type, handler]) => target.removeEventListener(type, handler));
        this.#listeners = [];

        for (const member of [...this.#members.keys()])
            member.detach();

        this.#members.clear();

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

        context.setPlaybackRate(this.#playbackRate);

        if (this.#debugInterval !== null)
            context.setDebugInterval(this.#debugInterval);

        if (this.#onDebugInfo !== null)
            context.onDebugInfo = this.#onDebugInfo;

        if (this.#transitionConfig !== null)
            context.setTransitionConfig(this.#transitionConfig);

        context.setBlendPattern(this.#blendPattern);

        if (this.#onPresetRequest !== null)
            context.onPresetRequest = this.#onPresetRequest;

        context.pause();
        this.#context = context;
        this.#lastFrameMs = 0;
    }

    #listen() {
        this.#addListener(this.#canvas, "webglcontextlost", event => this.#handleContextLost(event));
        this.#addListener(this.#canvas, "webglcontextrestored", () => this.#handleContextRestored());

        if (typeof document !== "undefined")
            this.#addListener(document, "visibilitychange", () => this.#handleVisibilityChange());

        this.#applyRunState();
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
            await this.#attach();
            this.#lost = false;
            this.#reloadPreset();
            this.#applyRunState();
        } catch (error) {
            this.#report(error);
        }
    }

    async #commitOnNextFrame(prepared, blendSeconds, preset, prepareMs) {
        const serial = ++this.#loadSerial;

        try {
            await new Promise(resolve => requestAnimationFrame(resolve));

            if (serial !== this.#loadSerial || this.#closed)
                return null;

            if (this.#context === null) {
                this.#preset = preset;
                return null;
            }

            const started = performance.now();
            this.#requireContext().commitPreset(prepared, blendSeconds);
            this.#preset = preset;
            return { prepareMs, commitMs: performance.now() - started };
        } finally {
            prepared.free();
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

    #visibleMembers() {
        return [...this.#members.keys()].filter(member => member.isVisible() && !member.isLost());
    }

    #applyRunState() {
        if (this.#context === null || this.#closed)
            return;

        makeWebGlContextCurrent(this.#module, this.#glHandle);

        if (this.#paused || this.#hidden || this.#visibleMembers().length === 0) {
            this.#context.pause();
            this.#stopLoop();
        } else {
            this.#context.resume();
            this.#startLoop();
        }
    }

    #fitSize(resizeBothWays = false) {
        if (this.#closed)
            return;

        const current = { width: this.#canvas.width, height: this.#canvas.height };
        let target;

        if (this.#fixedSize !== null) {
            target = this.#fixedSize;
        } else {
            const members = [...this.#members.keys()];

            if (members.length === 0)
                return;

            const width = Math.max(...members.map(member => member.canvas().width));
            const height = Math.max(...members.map(member => member.canvas().height));
            const scaled = {
                width: Math.max(1, Math.round(width * this.#renderScale)),
                height: Math.max(1, Math.round(height * this.#renderScale))
            };
            target = resizeBothWays ? scaled : {
                width: Math.max(current.width, scaled.width),
                height: Math.max(current.height, scaled.height)
            };

            if (current.width === INITIAL_SIZE && current.height === INITIAL_SIZE && this.#frameCount === 0 && this.#context !== null)
                target = scaled;
        }

        if (target.width === current.width && target.height === current.height)
            return;

        this.#applySize(target);

        if (this.#context === null)
            return;

        makeWebGlContextCurrent(this.#module, this.#glHandle);
        this.#context.resize(target.width, target.height);
    }

    #applySize(size) {
        this.#canvas.width = size.width;
        this.#canvas.height = size.height;
    }

    #startLoop() {
        if (this.#frameRequest !== 0 || this.#closed || this.#lost || this.#context === null)
            return;

        this.#lastFrameMs = 0;
        this.#nextDeadlineMs = 0;
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

        if (!this.#isFrameDue(now)) {
            this.#frameRequest = requestAnimationFrame(next => this.#frame(next));
            return;
        }

        const deltaSeconds = this.#lastFrameMs !== 0
            ? Math.min((now - this.#lastFrameMs) / MS_PER_SECOND, this.#maxFrameSeconds())
            : this.#firstFrameSeconds();
        this.#lastFrameMs = now;
        this.#advanceDeadline(now);

        try {
            this.#fitSize();
            makeWebGlContextCurrent(this.#module, this.#glHandle);

            if (this.#onBeforeRender !== null)
                this.#onBeforeRender(context, deltaSeconds);

            context.render(deltaSeconds, 0);
            this.#frameCount += 1;
            this.#present();
        } catch (error) {
            this.#report(error);
        }

        this.#frameRequest = requestAnimationFrame(next => this.#frame(next));
    }

    #present() {
        const presented = [];

        for (const member of this.#visibleMembers()) {
            try {
                this.#blit(member);
                presented.push(member.canvas());
            } catch (error) {
                this.#report(error);
            }
        }

        if (this.#onFramePresented !== null && presented.length > 0)
            this.#onFramePresented(presented, this.#frameCount);
    }

    #blit(member) {
        const entry = this.#members.get(member);
        const gl = member.gl();

        if (gl.isContextLost())
            return;

        if (entry.snapshot === null)
            entry.snapshot = SynesthesiaSyncGroup.#captureState(gl);

        if (entry.resources === null)
            entry.resources = SynesthesiaSyncGroup.#createBlitResources(gl);

        const target = member.canvas();
        const viewport = SynesthesiaSyncGroup.#viewportFor(member.scaling(), this.#canvas.width, this.#canvas.height, target.width, target.height);
        const { program, vertexArray, texture } = entry.resources;

        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.disable(gl.BLEND);
        gl.disable(gl.DEPTH_TEST);
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.CULL_FACE);
        gl.viewport(0, 0, target.width, target.height);

        if (viewport.width !== target.width || viewport.height !== target.height) {
            gl.clearColor(0, 0, 0, 1);
            gl.clear(gl.COLOR_BUFFER_BIT);
        }

        gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.#canvas);
        gl.useProgram(program);
        gl.bindVertexArray(vertexArray);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
    }

    static #viewportFor(scaling, sourceWidth, sourceHeight, targetWidth, targetHeight) {
        if (scaling === "stretch")
            return { x: 0, y: 0, width: targetWidth, height: targetHeight };

        const widthRatio = targetWidth / sourceWidth;
        const heightRatio = targetHeight / sourceHeight;
        const ratio = scaling === "fit" ? Math.min(widthRatio, heightRatio) : Math.max(widthRatio, heightRatio);
        const width = Math.round(sourceWidth * ratio);
        const height = Math.round(sourceHeight * ratio);
        return { x: Math.round((targetWidth - width) / 2), y: Math.round((targetHeight - height) / 2), width, height };
    }

    static #createBlitResources(gl) {
        const program = gl.createProgram();

        for (const [type, source] of [[gl.VERTEX_SHADER, BLIT_VERTEX_SOURCE], [gl.FRAGMENT_SHADER, BLIT_FRAGMENT_SOURCE]]) {
            const shader = gl.createShader(type);
            gl.shaderSource(shader, source);
            gl.compileShader(shader);

            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
                throw new SynesthesiaError(SynesthesiaError.SV_ERR_SHADER, "sync blit shader: " + gl.getShaderInfoLog(shader));

            gl.attachShader(program, shader);
            gl.deleteShader(shader);
        }

        gl.linkProgram(program);

        if (!gl.getProgramParameter(program, gl.LINK_STATUS))
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_SHADER, "sync blit program: " + gl.getProgramInfoLog(program));

        const texture = gl.createTexture();
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.useProgram(program);
        gl.uniform1i(gl.getUniformLocation(program, "source"), 0);
        return { program, vertexArray: gl.createVertexArray(), texture };
    }

    static #deleteBlitResources(gl, resources) {
        if (gl.isContextLost())
            return;

        gl.deleteProgram(resources.program);
        gl.deleteVertexArray(resources.vertexArray);
        gl.deleteTexture(resources.texture);
    }

    static #captureState(gl) {
        return {
            program: gl.getParameter(gl.CURRENT_PROGRAM),
            vertexArray: gl.getParameter(gl.VERTEX_ARRAY_BINDING),
            framebuffer: gl.getParameter(gl.FRAMEBUFFER_BINDING),
            activeTexture: gl.getParameter(gl.ACTIVE_TEXTURE),
            texture: gl.getParameter(gl.TEXTURE_BINDING_2D),
            viewport: gl.getParameter(gl.VIEWPORT),
            clearColor: gl.getParameter(gl.COLOR_CLEAR_VALUE),
            blend: gl.isEnabled(gl.BLEND),
            depthTest: gl.isEnabled(gl.DEPTH_TEST),
            scissorTest: gl.isEnabled(gl.SCISSOR_TEST),
            cullFace: gl.isEnabled(gl.CULL_FACE)
        };
    }

    static #restoreState(gl, state) {
        if (gl.isContextLost())
            return;

        const toggle = (capability, enabled) => enabled ? gl.enable(capability) : gl.disable(capability);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, state.texture);
        gl.activeTexture(state.activeTexture);
        gl.bindFramebuffer(gl.FRAMEBUFFER, state.framebuffer);
        gl.bindVertexArray(state.vertexArray);
        gl.useProgram(state.program);
        gl.viewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
        gl.clearColor(state.clearColor[0], state.clearColor[1], state.clearColor[2], state.clearColor[3]);
        toggle(gl.BLEND, state.blend);
        toggle(gl.DEPTH_TEST, state.depthTest);
        toggle(gl.SCISSOR_TEST, state.scissorTest);
        toggle(gl.CULL_FACE, state.cullFace);
    }

    #isFrameDue(now) {
        return this.#frameIntervalMs === 0 || now >= this.#nextDeadlineMs - this.#frameIntervalMs * DEADLINE_TOLERANCE;
    }

    #advanceDeadline(now) {
        if (this.#frameIntervalMs === 0)
            return;

        this.#nextDeadlineMs += this.#frameIntervalMs;

        if (this.#nextDeadlineMs < now)
            this.#nextDeadlineMs = now + this.#frameIntervalMs;
    }

    #maxFrameSeconds() {
        return this.#frameIntervalMs === 0
            ? MAX_FRAME_SECONDS
            : Math.max(MAX_FRAME_SECONDS, 2 * this.#frameIntervalMs / MS_PER_SECOND);
    }

    static #frameIntervalFor(maxFps) {
        if (!Number.isFinite(maxFps) || maxFps <= 0)
            return 0;

        return MS_PER_SECOND / Math.max(maxFps, MIN_MAX_FPS);
    }

    static #validScale(scale) {
        if (!Number.isFinite(scale) || scale <= 0)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "renderScale must be a positive number");

        return scale;
    }

    static #validSize(size) {
        if (size === null || size === undefined)
            return null;

        if (!Number.isInteger(size.width) || !Number.isInteger(size.height) || size.width < 1 || size.height < 1)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_ARG, "size needs a positive integer width and height");

        return { width: size.width, height: size.height };
    }

    #firstFrameSeconds() {
        const fps = this.#configuration.fpsHint ?? 0;
        return fps > 0 ? 1 / fps : DEFAULT_FRAME_SECONDS;
    }

    #requireContext() {
        if (this.#context === null)
            throw new SynesthesiaError(SynesthesiaError.SV_ERR_GL, this.#lost ? "the WebGL context is lost" : "SynesthesiaSyncGroup has no context");

        makeWebGlContextCurrent(this.#module, this.#glHandle);
        return this.#context;
    }

    #report(error) {
        const reported = error instanceof SynesthesiaError
            ? error
            : new SynesthesiaError(SynesthesiaError.SV_ERR_GL, error?.message ?? String(error));

        if (this.#onError !== null)
            this.#onError(reported);
        else
            console.error(reported);
    }
}
