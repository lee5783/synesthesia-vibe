//
//  gl_pipeline.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_GL_PIPELINE_H__
#define __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_GL_PIPELINE_H__ 1

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "audio_textures.h"
#include "blur_chain.h"
#include "disk_textures.h"
#include "draw_program.h"
#include "frame_targets.h"
#include "luminance_limiter.h"
#include "mesh.h"
#include "mesh_buffer.h"
#include "noise_textures.h"
#include "preset_shader_inputs.h"
#include "shader_translator.h"
#include "show_to_user_shaders.h"

struct sv_context;

struct BdPreparedStage
{
    bool translated;
    std::string hlsl;
    render_gl::TranslatedShader shader;
};

struct BdGlPipeline
{
    render_gl::FrameTargets targets;
    render_gl::DrawProgram texturedProgram;
    render_gl::DrawProgram untexturedProgram;
    render_gl::MeshBuffer warpMesh;
    render_gl::MeshBuffer quadMesh;
    render_gl::NoiseTextures noise;
    render_gl::BlurChain blur;
    render_gl::AudioTextures audio;
    render_gl::DiskTextureCache disk;
    render_gl::RandomTexturePicker picker;
    std::mt19937_64 pickRng;
    render_gl::PresetShaderCache presetCache;
    render_gl::PresetShaderInputs warpInputs;
    render_gl::PresetShaderInputs compInputs;
    render_gl::MeshBuffer compMesh;
    render_gl::CompositeGrid compGrid;
    render_gl::LuminanceLimiter limiter;

    std::vector<render_core::Vertex> warpVerts;

    std::string blurDumpDir;

    int highestBlurTexUsed;
    int presentBlurTexUsed;
    int forcedBlurLevels;
    int blurDumpFrame;
    int warpIndexCount;
    int glErrorCount;
    bool warpShaderActive;
    bool compShaderActive;
    bool resourcesBuilt;
    bool ready;
    bool limiterEnabled;
};

void BdGlPipelineCreate(sv_context* ctx);
void BdGlPipelineResize(sv_context* ctx);
void BdGlPipelineDestroy(sv_context* ctx);
std::string BdGlPreparePresetShaders(sv_context* ctx, const std::string& presetDir,
    const BdPreparedStage* preparedWarp = nullptr, const BdPreparedStage* preparedComp = nullptr,
    std::string* fallbackReason = nullptr);

void BdGlRenderFrame(sv_context* ctx, uint32_t targetFbo);
void BdGlBeginFrame(sv_context* ctx);
void BdGlStepFrame(sv_context* ctx);
void BdGlPresentFrame(sv_context* ctx, uint32_t targetFbo);

#endif
