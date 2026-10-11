//
//  preset_shader_inputs.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "preset_shader_inputs.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "gl_object_counts.h"
#include "rng_compat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

namespace render_gl
{

namespace
{

constexpr int kUniformNameMax = 256;
constexpr int kMaxUnits = 16;
constexpr int kRandomRotationFirst = 20;

struct ActiveUniform
{
    std::string name;
    unsigned int type;
    int location;
};

float RandomUnit()
{
    return (BdRand() % 7381) / 7380.0f;
}

void SetIdentity(float m[4][4])
{
    memset(m, 0, sizeof(float) * 16);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

void RotationX(float m[4][4], float angle)
{
    SetIdentity(m);
    m[1][1] = cosf(angle);
    m[1][2] = sinf(angle);
    m[2][1] = -sinf(angle);
    m[2][2] = cosf(angle);
}

void RotationY(float m[4][4], float angle)
{
    SetIdentity(m);
    m[0][0] = cosf(angle);
    m[0][2] = -sinf(angle);
    m[2][0] = sinf(angle);
    m[2][2] = cosf(angle);
}

void RotationZ(float m[4][4], float angle)
{
    SetIdentity(m);
    m[0][0] = cosf(angle);
    m[0][1] = sinf(angle);
    m[1][0] = -sinf(angle);
    m[1][1] = cosf(angle);
}

void Translation(float m[4][4], float x, float y, float z)
{
    SetIdentity(m);
    m[3][0] = x;
    m[3][1] = y;
    m[3][2] = z;
}

void Multiply(float out[4][4], const float a[4][4], const float b[4][4])
{
    float temp[4][4];

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            temp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }

    memcpy(out, temp, sizeof(temp));
}

void ComposeRotation(float out[4][4], float ax, float ay, float az, float tx, float ty, float tz)
{
    float mx[4][4];
    float my[4][4];
    float mz[4][4];
    float mt[4][4];

    RotationX(mx, ax);
    RotationY(my, ay);
    RotationZ(mz, az);
    Translation(mt, tx, ty, tz);

    Multiply(out, mx, mt);
    Multiply(out, out, mz);
    Multiply(out, out, my);
}

void ToColumnData(const float m[4][4], float data[12])
{
    for (int column = 0; column < 3; column++)
    {
        for (int row = 0; row < 4; row++)
            data[column * 4 + row] = m[row][column];
    }
}

void CollectActiveUniforms(unsigned int program, std::vector<ActiveUniform>& out)
{
    GLint count = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);

    for (GLint i = 0; i < count; i++)
    {
        char name[kUniformNameMax];
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(program, static_cast<GLuint>(i), kUniformNameMax, &length, &size, &type, name);

        std::string uniformName(name, static_cast<size_t>(length));
        const size_t bracket = uniformName.find('[');

        if (bracket != std::string::npos)
            uniformName.erase(bracket);

        if (uniformName.compare(0, 3, "gl_") == 0)
            continue;

        const int location = glGetUniformLocation(program, uniformName.c_str());

        if (location < 0)
            continue;

        out.push_back(ActiveUniform{ uniformName, type, location });
    }

    std::sort(out.begin(), out.end(), [](const ActiveUniform& a, const ActiveUniform& b) {
        return strcmp(a.name.c_str(), b.name.c_str()) < 0;
    });
}

const char* SourceName(SamplerSource source)
{
    switch (source)
    {
        case SamplerSource::Main: return "main";
        case SamplerSource::Noise: return "noise";
        case SamplerSource::Disk: return "disk";
        case SamplerSource::Blur: return "blur";
        case SamplerSource::Fft: return "fft";
        case SamplerSource::Wave: return "wave";
        default: return "missing";
    }
}

}

PresetShaderInputs::PresetShaderInputs()
    : m_program(nullptr)
    , m_posOffsetLocation(-1)
{
    memset(&m_resources, 0, sizeof(m_resources));
    memset(m_samplerObjects, 0, sizeof(m_samplerObjects));
}

PresetShaderInputs::~PresetShaderInputs()
{
    Destroy();
}

void PresetShaderInputs::Destroy()
{
    for (unsigned int& sampler : m_samplerObjects)
    {
        if (sampler != 0)
        {
            glDeleteSamplers(1, &sampler);
            NoteGlObjectDeleted(GlObjectKind::Sampler);
            sampler = 0;
        }
    }

    m_program = nullptr;
    m_posOffsetLocation = -1;
    memset(&m_resources, 0, sizeof(m_resources));
    m_samplers.clear();
    m_vectors.clear();
    m_matrices.clear();
    m_texSizeParams.clear();
    m_warnings.clear();
    m_activeNames.clear();
}

bool PresetShaderInputs::Resolve(const PresetProgram& program, const PresetShaderResources& resources)
{
    m_program = nullptr;
    m_samplers.clear();
    m_vectors.clear();
    m_matrices.clear();
    m_texSizeParams.clear();
    m_warnings.clear();
    m_activeNames.clear();

    if (program.shader == nullptr || !program.shader->Valid())
        return false;

    m_program = program.shader.get();
    m_posOffsetLocation = glGetUniformLocation(m_program->Id(), "uPosOffset");
    m_resources = resources;

    std::vector<ActiveUniform> uniforms;
    CollectActiveUniforms(m_program->Id(), uniforms);

    for (const ActiveUniform& uniform : uniforms)
        m_activeNames.push_back(uniform.name);

    render_core::RandomTextureSlots randSlots = {};
    std::map<std::string, std::pair<int, int>> textureSizes;
    std::vector<SamplerBindingInfo> resolved;

    for (const ActiveUniform& uniform : uniforms)
    {
        if (uniform.type != GL_SAMPLER_2D && uniform.type != GL_SAMPLER_3D)
            continue;

        render_core::SamplerResolution res;
        render_core::ResolveSamplerName(uniform.name.c_str(), kBlurTextureCount, resources.picker, randSlots, res);

        SamplerBindingInfo binding;
        binding.name = uniform.name;
        binding.rootName = res.rootName;
        binding.location = uniform.location;
        binding.unit = 0;
        binding.texCode = res.texCode;
        binding.source = SamplerSource::Missing;
        binding.is3D = uniform.type == GL_SAMPLER_3D;
        binding.wrap = res.bWrap;
        binding.bilinear = res.bBilinear;
        binding.sourceIndex = -1;
        binding.diskId = 0;
        binding.width = 0;
        binding.height = 0;

        switch (res.texCode)
        {
            case render_core::TexCode::Vs:
                binding.source = SamplerSource::Main;
                break;
            case render_core::TexCode::Blur1:
                binding.source = SamplerSource::Blur;
                binding.sourceIndex = 1;
                break;
            case render_core::TexCode::Blur2:
                binding.source = SamplerSource::Blur;
                binding.sourceIndex = 3;
                break;
            case render_core::TexCode::Blur3:
                binding.source = SamplerSource::Blur;
                binding.sourceIndex = 5;
                break;
            case render_core::TexCode::Fft:
                binding.source = SamplerSource::Fft;
                break;
            case render_core::TexCode::Wave:
                binding.source = SamplerSource::Wave;
                break;
            default:
            {
                const int noiseIndex = resources.noise != nullptr ? resources.noise->FindByName(res.rootName) : -1;

                if (noiseIndex >= 0)
                {
                    binding.source = SamplerSource::Noise;
                    binding.sourceIndex = noiseIndex;
                    binding.width = binding.height = resources.noise->Size(noiseIndex);
                    textureSizes[res.rootName] = std::make_pair(binding.width, binding.height);
                    break;
                }

                const DiskTexture* disk = resources.disk != nullptr ? resources.disk->Acquire(res.rootName) : nullptr;

                if (disk != nullptr)
                {
                    binding.source = SamplerSource::Disk;
                    binding.diskId = disk->id;
                    binding.width = disk->width;
                    binding.height = disk->height;
                    textureSizes[res.rootName] = std::make_pair(disk->width, disk->height);
                }
                else
                {
                    m_warnings.push_back(std::string("could not load texture \"") + res.rootName + "\" for " + uniform.name);
                }

                break;
            }
        }

        resolved.push_back(binding);
    }

    std::vector<std::pair<size_t, size_t>> order;

    for (size_t i = 0; i < resolved.size(); i++)
    {
        const size_t position = program.glsl.find(" " + resolved[i].name + ";");
        order.push_back(std::make_pair(position, i));
    }

    std::stable_sort(order.begin(), order.end());

    for (size_t rank = 0; rank < order.size(); rank++)
    {
        SamplerBindingInfo binding = resolved[order[rank].second];
        binding.unit = static_cast<int>(rank);

        if (binding.unit >= kMaxUnits)
        {
            m_warnings.push_back("too many samplers, dropping " + binding.name);
            continue;
        }

        m_samplers.push_back(binding);
    }

    for (const ActiveUniform& uniform : uniforms)
    {
        if (uniform.type == GL_FLOAT_MAT3x4)
        {
            const render_core::Float4ConstantResolution res =
                render_core::ResolveFloat4Constant(uniform.name.c_str(), true, false, randSlots);

            if (res.kind == render_core::Float4ConstantKind::RotMatrix)
                m_matrices.push_back(MatrixBinding{ uniform.name, uniform.location, res.index });
        }
        else if (uniform.type == GL_FLOAT_VEC4)
        {
            const render_core::Float4ConstantResolution res =
                render_core::ResolveFloat4Constant(uniform.name.c_str(), false, true, randSlots);

            if (res.kind == render_core::Float4ConstantKind::TexSize)
            {
                std::pair<int, int> size(0, 0);
                const int noiseIndex = resources.noise != nullptr ? resources.noise->FindByName(res.rootName) : -1;
                const auto known = textureSizes.find(res.rootName);

                if (noiseIndex >= 0)
                {
                    size = std::make_pair(resources.noise->Size(noiseIndex), resources.noise->Size(noiseIndex));
                }
                else if (known != textureSizes.end())
                {
                    size = known->second;
                }
                else
                {
                    m_warnings.push_back("unable to resolve texsize for a texture not in use: " + uniform.name);
                    continue;
                }

                m_vectors.push_back(Float4Binding{ uniform.name, uniform.location, res.kind, static_cast<int>(m_texSizeParams.size()) });
                m_texSizeParams.push_back(render_core::TexSizeConstInput{ size.first, size.second });
            }
            else if (res.kind != render_core::Float4ConstantKind::None)
            {
                m_vectors.push_back(Float4Binding{ uniform.name, uniform.location, res.kind, res.index });
            }
        }
    }

    return true;
}

unsigned int PresetShaderInputs::SamplerObject(bool bWrap, bool bBilinear, bool bMips)
{
    const int slot = (bWrap ? 1 : 0) | (bBilinear ? 2 : 0) | (bMips ? 4 : 0);

    if (m_samplerObjects[slot] == 0)
    {
        unsigned int sampler = 0;
        glGenSamplers(1, &sampler);
        NoteGlObjectCreated(GlObjectKind::Sampler);

        const GLint wrap = bWrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, wrap);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, bBilinear ? GL_LINEAR : GL_NEAREST);

        GLint minFilter = bBilinear ? GL_LINEAR : GL_NEAREST;

        if (bMips)
            minFilter = bBilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, minFilter);
        m_samplerObjects[slot] = sampler;
    }

    return m_samplerObjects[slot];
}

unsigned int PresetShaderInputs::TextureFor(const SamplerBindingInfo& binding, const PresetFrameInput& input, bool& bMips) const
{
    bMips = false;

    switch (binding.source)
    {
        case SamplerSource::Main:
            return input.mainTexture;
        case SamplerSource::Noise:
            bMips = true;
            return m_resources.noise->Id(binding.sourceIndex);
        case SamplerSource::Disk:
            bMips = true;
            return binding.diskId;
        case SamplerSource::Blur:
            return m_resources.blur->Level(binding.sourceIndex).ColorTexture().Id();
        case SamplerSource::Fft:
            return m_resources.audio->FftId();
        case SamplerSource::Wave:
            return m_resources.audio->WaveId();
        default:
            return 0;
    }
}

void PresetShaderInputs::Apply(const PresetFrameInput& input)
{
    if (m_program == nullptr)
        return;

    m_program->Bind();

    for (const SamplerBindingInfo& binding : m_samplers)
    {
        bool bMips = false;
        const unsigned int texture = TextureFor(binding, input, bMips);

        glActiveTexture(GL_TEXTURE0 + binding.unit);
        glBindTexture(binding.is3D ? GL_TEXTURE_3D : GL_TEXTURE_2D, texture);
        glBindSampler(binding.unit, SamplerObject(binding.wrap, binding.bilinear, bMips));
        glUniform1i(binding.location, binding.unit);

        if (binding.source == SamplerSource::Blur && input.highestBlurTexUsed != nullptr)
        {
            const int level = (static_cast<int>(binding.texCode) - static_cast<int>(render_core::TexCode::Blur1)) + 1;
            *input.highestBlurTexUsed = std::max(*input.highestBlurTexUsed, level);
        }
    }

    glActiveTexture(GL_TEXTURE0);

    render_core::ShaderConstantInput constInput;
    constInput.ctx = input.ctx;
    constInput.state = input.state;
    constInput.randFrame = input.randFrame;
    constInput.texSizeX = input.texSizeX;
    constInput.texSizeY = input.texSizeY;
    constInput.texSizeParams = m_texSizeParams;

    render_core::ShaderConstantBlock block;
    render_core::ComputeShaderConstants(constInput, block);

    for (const Float4Binding& binding : m_vectors)
    {
        const render_core::ShaderVec4* value = nullptr;

        switch (binding.kind)
        {
            case render_core::Float4ConstantKind::RandFrame:
                value = &block.randFrame;
                break;
            case render_core::Float4ConstantKind::RandPreset:
                value = &block.randPreset;
                break;
            case render_core::Float4ConstantKind::TexSize:
                value = &block.texSizeVectors[binding.index];
                break;
            case render_core::Float4ConstantKind::ConstVector:
                value = &block.consts[binding.index];
                break;
            case render_core::Float4ConstantKind::QConst:
                value = &block.qVectors[binding.index];
                break;
            default:
                break;
        }

        if (value != nullptr)
            glUniform4f(binding.location, value->x, value->y, value->z, value->w);
    }

    const double time = input.ctx->time - input.ctx->startTime;

    for (const MatrixBinding& binding : m_matrices)
    {
        float matrix[4][4];
        const int i = binding.index;

        if (i < kRandomRotationFirst)
        {
            const float ax = static_cast<float>(input.state->m_rot_base[i].x + input.state->m_rot_speed[i].x * time);
            const float ay = static_cast<float>(input.state->m_rot_base[i].y + input.state->m_rot_speed[i].y * time);
            const float az = static_cast<float>(input.state->m_rot_base[i].z + input.state->m_rot_speed[i].z * time);

            ComposeRotation(matrix, ax, ay, az, input.state->m_xlate[i].x, input.state->m_xlate[i].y, input.state->m_xlate[i].z);
        }
        else
        {
            const float ax = RandomUnit() * 6.28f;
            const float ay = RandomUnit() * 6.28f;
            const float az = RandomUnit() * 6.28f;
            const float tz = RandomUnit();
            const float ty = RandomUnit();
            const float tx = RandomUnit();

            ComposeRotation(matrix, ax, ay, az, tx, ty, tz);
        }

        float data[12];
        ToColumnData(matrix, data);
        glUniformMatrix3x4fv(binding.location, 1, GL_FALSE, data);
    }
}

bool PresetShaderInputs::UsesAudio() const
{
    for (const SamplerBindingInfo& binding : m_samplers)
    {
        if (binding.source == SamplerSource::Fft || binding.source == SamplerSource::Wave)
            return true;
    }

    return false;
}

void PresetShaderInputs::Restore()
{
    for (const SamplerBindingInfo& binding : m_samplers)
    {
        glActiveTexture(GL_TEXTURE0 + binding.unit);
        glBindSampler(binding.unit, 0);
        glBindTexture(binding.is3D ? GL_TEXTURE_3D : GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE0);
}

std::string PresetShaderInputs::Describe() const
{
    std::string text;
    char line[512];

    for (const SamplerBindingInfo& binding : m_samplers)
    {
        snprintf(line, sizeof(line), "sampler %-28s unit=%d src=%-7s root=%s wrap=%d bilinear=%d dim=%s size=%dx%d\n",
            binding.name.c_str(), binding.unit, SourceName(binding.source), binding.rootName.c_str(),
            binding.wrap ? 1 : 0, binding.bilinear ? 1 : 0, binding.is3D ? "3D" : "2D", binding.width, binding.height);
        text += line;
    }

    for (const Float4Binding& binding : m_vectors)
    {
        snprintf(line, sizeof(line), "vec4    %-28s kind=%d index=%d\n", binding.name.c_str(), static_cast<int>(binding.kind), binding.index);
        text += line;
    }

    for (const MatrixBinding& binding : m_matrices)
    {
        snprintf(line, sizeof(line), "mat3x4  %-28s index=%d\n", binding.name.c_str(), binding.index);
        text += line;
    }

    for (const std::string& warning : m_warnings)
        text += "warning " + warning + "\n";

    return text;
}

}
