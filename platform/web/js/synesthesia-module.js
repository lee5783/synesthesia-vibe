//
//  synesthesia-module.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

const WASM_GLUE = "./synesthesia-vibe-wasm.js";

export async function loadSynesthesiaModule(options = {}) {
    const glue = await import(new URL(WASM_GLUE, import.meta.url).href);
    return glue.default(options);
}
