//
//  preset_shader_inputs.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_PRESET_SHADER_INPUTS_H__
#define __BEATDROP_RENDER_GL_PRESET_SHADER_INPUTS_H__ 1

#include <string>
#include <vector>

#include "audio_textures.h"
#include "blur_chain.h"
#include "disk_textures.h"
#include "noise_textures.h"
#include "sampler_resolver.h"
#include "shader_constants.h"
#include "shader_translator.h"

namespace render_gl
{

enum class SamplerSource
{
    Main,
    Noise,
    Disk,
    Blur,
    Fft,
    Wave,
    Missing
};

struct SamplerBindingInfo
{
    std::string name;
    std::string rootName;
    int location;
    int unit;
    render_core::TexCode texCode;
    SamplerSource source;
    bool is3D;
    bool wrap;
    bool bilinear;
    int sourceIndex;
    unsigned int diskId;
    int width;
    int height;
};

struct PresetShaderResources
{
    const NoiseTextures* noise;
    const BlurChain* blur;
    const AudioTextures* audio;
    DiskTextureCache* disk;
    render_core::IRandomTextureSource* picker;
};

struct PresetFrameInput
{
    const FrameContext* ctx;
    CState* state;
    render_core::ShaderVec4 randFrame;
    int texSizeX;
    int texSizeY;
    unsigned int mainTexture;
    int* highestBlurTexUsed;
};

class PresetShaderInputs
{
public:
    PresetShaderInputs();
    ~PresetShaderInputs();
    PresetShaderInputs(const PresetShaderInputs&) = delete;
    PresetShaderInputs& operator=(const PresetShaderInputs&) = delete;

    bool Resolve(const PresetProgram& program, const PresetShaderResources& resources);
    void Apply(const PresetFrameInput& input);
    void Restore();
    void Destroy();

    bool Resolved() const { return m_program != nullptr; }
    const Shader* Program() const { return m_program; }
    int PosOffsetLocation() const { return m_posOffsetLocation; }
    bool UsesAudio() const;
    const std::vector<SamplerBindingInfo>& Samplers() const { return m_samplers; }
    const std::vector<std::string>& Warnings() const { return m_warnings; }
    const std::vector<std::string>& ActiveNames() const { return m_activeNames; }
    std::string Describe() const;

private:
    struct Float4Binding
    {
        std::string name;
        int location;
        render_core::Float4ConstantKind kind;
        int index;
    };

    struct MatrixBinding
    {
        std::string name;
        int location;
        int index;
    };

    unsigned int SamplerObject(bool bWrap, bool bBilinear, bool bMips);
    unsigned int TextureFor(const SamplerBindingInfo& binding, const PresetFrameInput& input, bool& bMips) const;

    const Shader* m_program;
    int m_posOffsetLocation;
    PresetShaderResources m_resources;
    std::vector<SamplerBindingInfo> m_samplers;
    std::vector<Float4Binding> m_vectors;
    std::vector<MatrixBinding> m_matrices;
    std::vector<render_core::TexSizeConstInput> m_texSizeParams;
    std::vector<std::string> m_warnings;
    std::vector<std::string> m_activeNames;
    unsigned int m_samplerObjects[8];
};

}

#endif
