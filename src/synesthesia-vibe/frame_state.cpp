//
//  frame_state.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "context.h"

namespace
{
    std::string BdJsonFloat32(float value)
    {
        unsigned int bits = 0;
        memcpy(&bits, &value, sizeof(bits));

        char buf[64];
        snprintf(buf, sizeof(buf), "{\"hex\":\"0x%08X\",\"dec\":%.9g}", bits, static_cast<double>(value));
        return std::string(buf);
    }

    std::string BdJsonFloat64(double value)
    {
        unsigned long long bits = 0;
        memcpy(&bits, &value, sizeof(bits));

        char buf[80];
        snprintf(buf, sizeof(buf), "{\"hex\":\"0x%016llX\",\"dec\":%.17g}", bits, value);
        return std::string(buf);
    }

    std::string BdJsonVec4(float x, float y, float z, float w)
    {
        std::string out("[");
        out += BdJsonFloat32(x);
        out += ",";
        out += BdJsonFloat32(y);
        out += ",";
        out += BdJsonFloat32(z);
        out += ",";
        out += BdJsonFloat32(w);
        out += "]";
        return out;
    }

    void BdRefreshMeshTexcoords(sv_context* ctx)
    {
        if (ctx->state == nullptr || ctx->state->var_pf_zoom == nullptr)
            return;

        if (ctx->meshVerts.empty() || ctx->meshVertInfo.empty())
            return;

        render_core::GridAlphaInput input;
        input.state = ctx->state;
        input.oldState = ctx->oldState;
        input.time = static_cast<float>(ctx->timeModel.GetTime());
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectX = ctx->aspectX;
        input.aspectY = ctx->aspectY;
        input.invAspectX = ctx->invAspectX;
        input.invAspectY = ctx->invAspectY;
        input.texSizeX = ctx->texsizeX;
        input.texSizeY = ctx->texsizeY;
        input.gridX = ctx->nGridX;
        input.gridY = ctx->nGridY;

        render_core::ComputeGridAlphaValues(input, ctx->meshVerts.data(), ctx->meshVertInfo.data());
    }
}

extern "C" void sv_dump_frame_state(sv_context* ctx, const char* json_path)
{
    if (ctx == nullptr || json_path == nullptr || json_path[0] == '\0')
        return;

    CState* pState = ctx->state;

    float blurMin[3] = { 0.0f, 0.0f, 0.0f };
    float blurMax[3] = { 0.0f, 0.0f, 0.0f };
    if (pState != nullptr && pState->var_pf_blur1min != nullptr)
        render_core::GetSafeBlurMinMax(pState, blurMin, blurMax);

    const float c3w = 0.3333f * ctx->mysound.imm_rel[2];
    const float c4w = 0.3333f * ctx->mysound.avg_rel[2];

    const int renderedFrame = ctx->frame > 0 ? ctx->frame - 1 : 0;

    BdRefreshMeshTexcoords(ctx);

    std::ofstream out(json_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open())
        return;

    out << "{\n";
    out << "  \"frame\": " << renderedFrame << ",\n";
    out << "  \"time\": " << BdJsonFloat32(static_cast<float>(ctx->timeModel.GetTime())) << ",\n";
    out << "  \"fps\": " << BdJsonFloat32(ctx->timeModel.GetFps()) << ",\n";

    out << "  \"rand_preset\": " << BdJsonVec4(pState->m_rand_preset.x, pState->m_rand_preset.y,
        pState->m_rand_preset.z, pState->m_rand_preset.w) << ",\n";
    out << "  \"rand_frame\": " << BdJsonVec4(ctx->randFrame.x, ctx->randFrame.y,
        ctx->randFrame.z, ctx->randFrame.w) << ",\n";

    out << "  \"sound\": {\n";
    out << "    \"bass\": " << BdJsonFloat32(ctx->mysound.imm_rel[0]) << ",\n";
    out << "    \"mid\": " << BdJsonFloat32(ctx->mysound.imm_rel[1]) << ",\n";
    out << "    \"treb\": " << BdJsonFloat32(ctx->mysound.imm_rel[2]) << ",\n";
    out << "    \"bass_att\": " << BdJsonFloat32(ctx->mysound.avg_rel[0]) << ",\n";
    out << "    \"mid_att\": " << BdJsonFloat32(ctx->mysound.avg_rel[1]) << ",\n";
    out << "    \"treb_att\": " << BdJsonFloat32(ctx->mysound.avg_rel[2]) << "\n";
    out << "  },\n";

    out << "  \"_c3\": " << BdJsonVec4(ctx->mysound.imm_rel[0], ctx->mysound.imm_rel[1],
        ctx->mysound.imm_rel[2], c3w) << ",\n";
    out << "  \"_c4\": " << BdJsonVec4(ctx->mysound.avg_rel[0], ctx->mysound.avg_rel[1],
        ctx->mysound.avg_rel[2], c4w) << ",\n";

    out << "  \"blur_min\": [" << BdJsonFloat32(blurMin[0]) << "," << BdJsonFloat32(blurMin[1])
        << "," << BdJsonFloat32(blurMin[2]) << "],\n";
    out << "  \"blur_max\": [" << BdJsonFloat32(blurMax[0]) << "," << BdJsonFloat32(blurMax[1])
        << "," << BdJsonFloat32(blurMax[2]) << "],\n";

    out << "  \"q\": [";
    for (int i = 0; i < NUM_Q_VAR; ++i)
    {
        if (i > 0)
            out << ",";
        const double* q = pState->var_pf_q[i];
        out << BdJsonFloat64(q != nullptr ? *q : 0.0);
    }
    out << "],\n";

    out << "  \"custom_waves\": [";
    for (int i = 0; i < MAX_CUSTOM_WAVES; ++i)
    {
        if (i > 0)
            out << ",";
        const CWave& wave = pState->m_wave[i];
        out << "{\"index\":" << i << ",\"enabled\":" << (wave.enabled ? "true" : "false") << ",\"t\":[";
        for (int t = 0; t < NUM_T_VAR; ++t)
        {
            if (t > 0)
                out << ",";
            const double* tv = wave.var_pf_t[t];
            out << BdJsonFloat64(tv != nullptr ? *tv : 0.0);
        }
        out << "]}";
    }
    out << "],\n";

    out << "  \"custom_shapes\": [";
    for (int i = 0; i < MAX_CUSTOM_SHAPES; ++i)
    {
        if (i > 0)
            out << ",";
        const CShape& shape = pState->m_shape[i];
        out << "{\"index\":" << i << ",\"enabled\":" << (shape.enabled ? "true" : "false") << ",\"t\":[";
        for (int t = 0; t < NUM_T_VAR; ++t)
        {
            if (t > 0)
                out << ",";
            const double* tv = shape.var_pf_t[t];
            out << BdJsonFloat64(tv != nullptr ? *tv : 0.0);
        }
        out << "]}";
    }
    out << "],\n";

    const int nVertsX = ctx->nGridX + 1;
    const int nVertsY = ctx->nGridY + 1;
    out << "  \"mesh\": {\"grid_x\": " << ctx->nGridX << ", \"grid_y\": " << ctx->nGridY << ", \"verts\": [";
    for (int i = 0; i < nVertsX * nVertsY; ++i)
    {
        if (i > 0)
            out << ",";
        const render_core::Vertex& v = ctx->meshVerts[static_cast<size_t>(i)];
        out << "{\"x\":" << BdJsonFloat32(v.x) << ",\"y\":" << BdJsonFloat32(v.y)
            << ",\"z\":" << BdJsonFloat32(v.z)
            << ",\"diffuse\":" << v.Diffuse
            << ",\"tu\":" << BdJsonFloat32(v.tu) << ",\"tv\":" << BdJsonFloat32(v.tv)
            << ",\"tu_orig\":" << BdJsonFloat32(v.tu_orig) << ",\"tv_orig\":" << BdJsonFloat32(v.tv_orig)
            << ",\"rad\":" << BdJsonFloat32(v.rad) << ",\"ang\":" << BdJsonFloat32(v.ang) << "}";
    }
    out << "]},\n";

    out << "  \"passes\": []\n";

    out << "}\n";

    out.close();
}