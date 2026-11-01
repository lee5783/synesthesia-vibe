//
//  webgl-context.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

import { SynesthesiaError } from "./synesthesia-error.js";

export const CONTEXT_ATTRIBUTES = Object.freeze({
    alpha: false,
    antialias: false,
    premultipliedAlpha: false,
    depth: false,
    stencil: false,
    preserveDrawingBuffer: false
});

const EMSCRIPTEN_ATTRIBUTES = Object.freeze({
    ...CONTEXT_ATTRIBUTES,
    majorVersion: 2,
    minorVersion: 0,
    enableExtensionsByDefault: true
});

export function createWebGl2Context(canvas) {
    const gl = canvas.getContext("webgl2", CONTEXT_ATTRIBUTES);

    if (gl === null)
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_GL, "WebGL2 is not available on this canvas");

    return gl;
}

export function registerWebGlContext(module, gl) {
    const handle = module.GL.registerContext(gl, EMSCRIPTEN_ATTRIBUTES);

    if (!handle || !module.GL.makeContextCurrent(handle))
        throw new SynesthesiaError(SynesthesiaError.SV_ERR_GL, "the WebGL2 context could not be made current");

    return handle;
}

export function makeWebGlContextCurrent(module, handle) {
    module.GL.makeContextCurrent(handle);
}

export function releaseWebGlContext(module, handle) {
    if (handle && module.GL.contexts[handle])
        module.GL.deleteContext(handle);
}
