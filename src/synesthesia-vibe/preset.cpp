//
//  preset.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include "context.h"
#include "gl_pipeline.h"
#include "parser_win32.h"
#include "shader_text.h"

namespace
{
    const char kIncludeResourceName[] = "data/include.fx";
    const char kWarpResourceName[] = "data/warp_ps.fx";
    const char kCompResourceName[] = "data/comp_ps.fx";

    constexpr size_t kBeatdropMaxNativePath = 260;

    std::wstring BdToNativePath(const std::wstring& path)
    {
#ifdef _WIN32
        std::wstring native(path);

        if (native.size() >= kBeatdropMaxNativePath && native.rfind(L"\\\\?\\", 0) != 0)
        {
            if (native.rfind(L"\\\\", 0) == 0)
                native = L"\\\\?\\UNC\\" + native.substr(2);
            else
                native = L"\\\\?\\" + native;
        }

        return native;
#else
        return path;
#endif
    }

    void BdLoadShaderResources(sv_context* ctx)
    {
        const std::string& root = ctx->resourcePath;

        ctx->shaderIncludeText[0] = 0;
        ctx->defaultWarpShaderText[0] = 0;
        ctx->defaultCompShaderText[0] = 0;

        render_core::ReadResourceToString(root.c_str(), kIncludeResourceName, ctx->shaderIncludeText,
            kBeatdropResourceTextBytes, false);
        render_core::StripComments(ctx->shaderIncludeText);

        render_core::ReadResourceToString(root.c_str(), kWarpResourceName, ctx->defaultWarpShaderText,
            kBeatdropResourceTextBytes, true);
        render_core::ReadResourceToString(root.c_str(), kCompResourceName, ctx->defaultCompShaderText,
            kBeatdropResourceTextBytes, true);
    }

    std::string BdPresetDirectory(const char* path)
    {
        const std::string full(path);
        const size_t separator = full.find_last_of("/\\");

        if (separator == std::string::npos)
            return std::string();

        return full.substr(0, separator);
    }

    void BdSetPresetError(sv_context* ctx, const std::string& message)
    {
        ctx->lastError = message;
    }

    void BdApplyPresetImport(sv_context* ctx, const std::wstring& presetPath, float blendSeconds)
    {
        ctx->hostSink.Clear();

        if (wcscmp(ctx->state->m_szDesc, INVALID_PRESET_DESC) == 0)
            blendSeconds = 0.0f;

        CState* temp = ctx->state;
        ctx->state = ctx->oldState;
        ctx->oldState = temp;

        const FrameInputs frameInputs = BdMakeFrameInputs(ctx);
        ParserCapabilities caps;
        caps.isAMD = false;
        caps.maxPSVersion = MD2_PS_3_0;

        const float now = static_cast<float>(ctx->timeModel.GetTime());

        ctx->state->Import(presetPath.c_str(), now, ctx->oldState, STATE_ALL, caps,
            ctx->defaultWarpShaderText, ctx->defaultCompShaderText, frameInputs, ctx->hostSink);

        if (blendSeconds >= 0.001f)
            ctx->state->StartBlendFrom(ctx->oldState, now, blendSeconds);

        ctx->presetStartTime = now;
        ctx->nextPresetTime = -1.0f;
        ctx->nextPresetTimeWasInitialized = false;
    }
}

static sv_status BdLoadPresetFrom(sv_context* ctx, const char* path, float blend_seconds, const std::string& textureDirectory)
{
    if (ctx == nullptr || path == nullptr || path[0] == '\0')
    {
        if (ctx != nullptr)
            BdSetPresetError(ctx, "sv_load_preset: path is empty");
        return SV_ERR_ARG;
    }

    BdLoadShaderResources(ctx);

    const std::wstring presetPath = BdUtf8ToWide(path);
    const std::wstring nativePath = BdToNativePath(presetPath);

    if (!BdFileExists(nativePath.c_str()))
    {
        std::string message("preset not found: ");
        message += path;
        BdSetPresetError(ctx, message);
        return SV_ERR_IO;
    }

    const float previousPresetStartTime = ctx->presetStartTime;
    const float previousNextPresetTime = ctx->nextPresetTime;
    const bool previousNextPresetTimeWasInitialized = ctx->nextPresetTimeWasInitialized;

    BdApplyPresetImport(ctx, nativePath, blend_seconds);

    if (ctx->hostSink.ErrorCount() > 0)
    {
        CState* failed = ctx->state;
        ctx->state = ctx->oldState;
        ctx->oldState = failed;

        ctx->presetStartTime = previousPresetStartTime;
        ctx->nextPresetTime = previousNextPresetTime;
        ctx->nextPresetTimeWasInitialized = previousNextPresetTimeWasInitialized;

        BdSetPresetError(ctx, ctx->hostSink.LastError());
        return SV_ERR_PARSE;
    }

    ctx->lastError = BdGlPreparePresetShaders(ctx, textureDirectory);

    if (ctx->gl != nullptr && !ctx->gl->ready)
    {
        ctx->lastError = "GL pipeline unavailable, preset loaded but cannot render";
        return SV_ERR_GL;
    }

    return SV_OK;
}

extern "C" sv_status sv_load_preset(sv_context* ctx, const char* path, float blend_seconds)
{
    if (path == nullptr)
        return BdLoadPresetFrom(ctx, path, blend_seconds, std::string());

    return BdLoadPresetFrom(ctx, path, blend_seconds, BdPresetDirectory(path));
}

extern "C" sv_status sv_load_preset_memory(sv_context* ctx, const char* text, size_t len, float blend_seconds)
{
    if (ctx == nullptr || text == nullptr)
    {
        if (ctx != nullptr)
            BdSetPresetError(ctx, "sv_load_preset_memory: text is null");
        return SV_ERR_ARG;
    }

    char tempName[64];
    snprintf(tempName, sizeof(tempName), "bd-preset-memory-%p.tmp", static_cast<const void*>(ctx));

    std::string tempPath = ctx->cachePath;

    if (tempPath.empty())
    {
        const char* env = getenv("TEMP");
        if (env == nullptr || env[0] == 0)
            env = getenv("TMPDIR");

        if (env != nullptr && env[0] != 0)
            tempPath = env;
        else
            tempPath = ".";
    }

    tempPath += "/";
    tempPath += tempName;

    FILE* f = fopen(tempPath.c_str(), "wb");
    if (f == nullptr)
    {
        BdSetPresetError(ctx, "sv_load_preset_memory: could not create temporary file");
        return SV_ERR_IO;
    }

    if (len > 0)
        fwrite(text, 1, len, f);
    fclose(f);

    const sv_status status = BdLoadPresetFrom(ctx, tempPath.c_str(), blend_seconds, ctx->textureSearchPath);

    remove(tempPath.c_str());
    return status;
}