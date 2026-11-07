//
//  preset.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "context.h"
#include "gl_pipeline.h"
#include "parser_win32.h"
#include "shader_text.h"

void GetFast_CLEAR();
int GetFastInt(const char* szVarName, int def, FILE* f);
void ReadCode(FILE* f, char* pStr, char* prefix);

struct sv_prepared_preset
{
    sv_context* owner;
    uint64_t ownerSerial;
    std::unique_ptr<CState> state;
    BdPreparedStage warp;
    BdPreparedStage comp;
    int presetVersion;
    std::string presetName;
    std::string textureDirectory;
    bool committed;
};

namespace
{
    const char kNotPresetMessage[] = "not a MilkDrop preset: no [preset00] section or known key";
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

    void BdLoadShaderResources(const std::string& root, char* includeText, char* defaultWarpText, char* defaultCompText)
    {
        includeText[0] = 0;
        defaultWarpText[0] = 0;
        defaultCompText[0] = 0;

        render_core::ReadResourceToString(root.c_str(), kIncludeResourceName, includeText,
            kBeatdropResourceTextBytes, false);
        render_core::StripComments(includeText);

        render_core::ReadResourceToString(root.c_str(), kWarpResourceName, defaultWarpText,
            kBeatdropResourceTextBytes, true);
        render_core::ReadResourceToString(root.c_str(), kCompResourceName, defaultCompText,
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

    const char kMilk2Message[] = ".milk2 (MilkDrop 3 double preset) is not supported";

    bool BdHasMilk2Extension(const char* path)
    {
        const size_t length = strlen(path);
        static const char extension[] = ".milk2";
        const size_t extensionLength = sizeof(extension) - 1;

        if (length < extensionLength)
            return false;

        for (size_t i = 0; i < extensionLength; i++)
        {
            char c = path[length - extensionLength + i];

            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');

            if (c != extension[i])
                return false;
        }

        return true;
    }

    bool BdStartsWith(const char* text, size_t len, const char* prefix)
    {
        const size_t prefixLen = strlen(prefix);
        return len >= prefixLen && memcmp(text, prefix, prefixLen) == 0;
    }

    bool BdLooksLikePresetText(const char* text, size_t len)
    {
        static const char* const kMarkers[] = {
            "[preset",
            "MILKDROP_PRESET_VERSION",
            "fRating",
            "per_frame_",
            "per_pixel_",
        };

        size_t index = 0;

        if (BdStartsWith(text, len, "\xEF\xBB\xBF"))
            index = 3;

        while (index < len)
        {
            const void* newline = memchr(text + index, '\n', len - index);
            const size_t lineEnd = newline != nullptr ? static_cast<size_t>(static_cast<const char*>(newline) - text) : len;

            size_t begin = index;
            while (begin < lineEnd && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r'))
                begin++;

            for (const char* szMarker : kMarkers)
            {
                if (BdStartsWith(text + begin, lineEnd - begin, szMarker))
                    return true;
            }

            index = lineEnd + 1;
        }

        return false;
    }

    bool BdReadFileText(const std::wstring& nativePath, std::string& text)
    {
        FILE* f = ParserWfopen(nativePath.c_str(), L"rb");
        if (f == nullptr)
            return false;

        char buffer[4096];
        size_t got = 0;

        while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0)
            text.append(buffer, got);

        fclose(f);
        return true;
    }

    bool BdFileLooksLikePreset(const std::wstring& nativePath)
    {
        std::string text;
        return BdReadFileText(nativePath, text) && BdLooksLikePresetText(text.data(), text.size());
    }

    std::string BdTempDirectory(const std::string& preferred)
    {
        if (!preferred.empty())
            return preferred;

        const char* env = getenv("TEMP");
        if (env == nullptr || env[0] == 0)
            env = getenv("TMPDIR");

        if (env != nullptr && env[0] != 0)
            return env;

        return ".";
    }

    bool BdWriteTempFile(const std::string& tempPath, const char* text, size_t len)
    {
        FILE* f = fopen(tempPath.c_str(), "wb");
        if (f == nullptr)
            return false;

        if (len > 0)
            fwrite(text, 1, len, f);
        fclose(f);
        return true;
    }

    void BdCopyError(char* error, size_t errorLen, const std::string& message)
    {
        if (error == nullptr || errorLen == 0)
            return;

        BdCopyString(error, static_cast<int>(errorLen > static_cast<size_t>(INT_MAX) ? INT_MAX : errorLen), message.c_str());
    }

    int BdPresetShaderVersion(FILE* f, int presetVersion, const char* key)
    {
        if (presetVersion < 200)
            return 0;

        if (presetVersion == 200)
            return GetFastInt("PSVERSION", 2, f);

        return GetFastInt(key, 2, f);
    }

    struct BdPresetCode
    {
        NSEEL_VMCTX vm;
        char* text;
        std::string label;
    };

    std::vector<BdPresetCode> BdCollectPresetCode(CState& state)
    {
        std::vector<BdPresetCode> code;
        code.push_back({ state.m_pf_eel, state.m_szPerFrameInit, "per_frame_init" });
        code.push_back({ state.m_pf_eel, state.m_szPerFrameExpr, "per_frame" });
        code.push_back({ state.m_pv_eel, state.m_szPerPixelExpr, "per_pixel" });

        for (int i = 0; i < MAX_CUSTOM_WAVES; i++)
        {
            const std::string prefix = "wave_" + std::to_string(i);
            code.push_back({ state.m_wave[i].m_pf_eel, state.m_wave[i].m_szInit, prefix + "_init" });
            code.push_back({ state.m_wave[i].m_pf_eel, state.m_wave[i].m_szPerFrame, prefix + "_per_frame" });
            code.push_back({ state.m_wave[i].m_pp_eel, state.m_wave[i].m_szPerPoint, prefix + "_per_point" });
        }

        for (int i = 0; i < MAX_CUSTOM_SHAPES; i++)
        {
            const std::string prefix = "shape_" + std::to_string(i);
            code.push_back({ state.m_shape[i].m_pf_eel, state.m_shape[i].m_szInit, prefix + "_init" });
            code.push_back({ state.m_shape[i].m_pf_eel, state.m_shape[i].m_szPerFrame, prefix + "_per_frame" });
        }

        return code;
    }

    void BdReadPresetCode(FILE* f, CState& state, int& warpVersion, int& compVersion)
    {
        GetFast_CLEAR();

        const int presetVersion = GetFastInt("MILKDROP_PRESET_VERSION", 100, f);
        warpVersion = BdPresetShaderVersion(f, presetVersion, "PSVERSION_WARP");
        compVersion = BdPresetShaderVersion(f, presetVersion, "PSVERSION_COMP");

        char prefix[64];

        snprintf(prefix, sizeof(prefix), "per_frame_init_");
        ReadCode(f, state.m_szPerFrameInit, prefix);
        snprintf(prefix, sizeof(prefix), "per_frame_");
        ReadCode(f, state.m_szPerFrameExpr, prefix);
        snprintf(prefix, sizeof(prefix), "per_pixel_");
        ReadCode(f, state.m_szPerPixelExpr, prefix);
        snprintf(prefix, sizeof(prefix), "warp_");
        ReadCode(f, state.m_szWarpShadersText, prefix);
        snprintf(prefix, sizeof(prefix), "comp_");
        ReadCode(f, state.m_szCompShadersText, prefix);

        for (int i = 0; i < MAX_CUSTOM_WAVES; i++)
        {
            snprintf(prefix, sizeof(prefix), "wave_%d_init", i);
            ReadCode(f, state.m_wave[i].m_szInit, prefix);
            snprintf(prefix, sizeof(prefix), "wave_%d_per_frame", i);
            ReadCode(f, state.m_wave[i].m_szPerFrame, prefix);
            snprintf(prefix, sizeof(prefix), "wave_%d_per_point", i);
            ReadCode(f, state.m_wave[i].m_szPerPoint, prefix);
        }

        for (int i = 0; i < MAX_CUSTOM_SHAPES; i++)
        {
            snprintf(prefix, sizeof(prefix), "shape_%d_init", i);
            ReadCode(f, state.m_shape[i].m_szInit, prefix);
            snprintf(prefix, sizeof(prefix), "shape_%d_per_frame", i);
            ReadCode(f, state.m_shape[i].m_szPerFrame, prefix);
        }

        GetFast_CLEAR();
    }

    int BdReadPresetVersion(const std::wstring& nativePath)
    {
        FILE* f = ParserWfopen(nativePath.c_str(), L"rb");
        if (f == nullptr)
            return 0;

        GetFast_CLEAR();
        const int presetVersion = GetFastInt("MILKDROP_PRESET_VERSION", 100, f);
        GetFast_CLEAR();

        fclose(f);
        return presetVersion;
    }

    bool BdIsBlankCode(const char* text)
    {
        for (const char* p = text; *p != 0; p++)
        {
            if (*p != ' ')
                return false;
        }

        return true;
    }

    bool BdCompilePresetCode(CState& state, std::string& message)
    {
        std::vector<char> buffer(MAX_BIGSTRING_LEN * 3);

        for (const BdPresetCode& code : BdCollectPresetCode(state))
        {
            state.StripLinefeedCharsAndComments(code.text, buffer.data());

            if (BdIsBlankCode(buffer.data()))
                continue;

            NSEEL_CODEHANDLE handle = NSEEL_code_compile(code.vm, buffer.data());

            if (handle == nullptr)
            {
                const char* szError = NSEEL_code_getcodeerror(code.vm);
                message = "preset code error in " + code.label;

                if (szError != nullptr && szError[0] != 0)
                    message += std::string(": ") + szError;

                return false;
            }

            NSEEL_code_free(handle);
        }

        return true;
    }

    bool BdTranslateStage(render_gl::PresetShaderStage stage, const char* stageName, int version, const char* text,
        std::string& message)
    {
        if (version <= 0 || text[0] == 0)
            return true;

        const render_gl::TranslatedShader translated = render_gl::TranslatePresetShader(stage, text);

        if (translated.ok)
            return true;

        message = std::string(stageName) + " shader translation failed: " + translated.error;
        return false;
    }

    sv_status BdValidatePresetFile(const char* path, std::string& message)
    {
        if (path == nullptr || path[0] == '\0')
        {
            message = "sv_validate_preset: path is empty";
            return SV_ERR_ARG;
        }

        if (BdHasMilk2Extension(path))
        {
            message = kMilk2Message;
            return SV_ERR_PARSE;
        }

        const std::wstring nativePath = BdToNativePath(BdUtf8ToWide(path));

        if (!BdFileExists(nativePath.c_str()))
        {
            message = std::string("preset not found: ") + path;
            return SV_ERR_IO;
        }

        if (!BdFileLooksLikePreset(nativePath))
        {
            message = kNotPresetMessage;
            return SV_ERR_PARSE;
        }

        FILE* f = ParserWfopen(nativePath.c_str(), L"rb");
        if (f == nullptr)
        {
            message = std::string("preset not readable: ") + path;
            return SV_ERR_IO;
        }

        std::unique_ptr<CState> state(new CState());
        state->FreeVarsAndCode();

        int warpVersion = 0;
        int compVersion = 0;
        BdReadPresetCode(f, *state, warpVersion, compVersion);
        fclose(f);

        if (!BdCompilePresetCode(*state, message))
            return SV_ERR_PARSE;

        if (!BdTranslateStage(render_gl::PresetShaderStage::Warp, "warp", warpVersion, state->m_szWarpShadersText, message) ||
            !BdTranslateStage(render_gl::PresetShaderStage::Composite, "comp", compVersion, state->m_szCompShadersText, message))
            return SV_ERR_SHADER;

        message.clear();
        return SV_OK;
    }

    void BdStartPresetBlend(sv_context* ctx, float now, float blendSeconds)
    {
        if (blendSeconds >= 0.001f)
            ctx->state->StartBlendFrom(ctx->oldState, now, blendSeconds);

        ctx->presetStartTime = now;
        ctx->nextPresetTime = -1.0f;
        ctx->nextPresetTimeWasInitialized = false;
    }

    void BdRandomizeBlendPattern(sv_context* ctx)
    {
        render_core::BlendPatternInput input;
        input.pattern = ctx->blendPattern;
        input.gridX = ctx->nGridX;
        input.gridY = ctx->nGridY;
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectX = ctx->aspectX;
        input.aspectY = ctx->aspectY;
        input.fps = ctx->timeModel.GetFps();

        render_core::RandomizeBlendPattern(input, ctx->blendPatternClock, ctx->rng, ctx->meshVertInfo.data());
    }

    float BdApplyPresetImport(sv_context* ctx, const std::wstring& presetPath, float blendSeconds)
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

        BdStartPresetBlend(ctx, now, blendSeconds);
        return blendSeconds;
    }

    void BdPrepareStage(render_gl::TranslatedShaderCache& translations, render_gl::PresetShaderStage stage, int version,
        const char* text, BdPreparedStage& prepared)
    {
        prepared.translated = version > 0;

        if (!prepared.translated)
            return;

        prepared.hlsl = text;
        prepared.shader = translations.GetOrTranslate(stage, text);
    }

    sv_status BdPreparePresetFile(sv_context* ctx, const char* path, const std::string& textureDirectory, bool reportPresetName,
        std::unique_ptr<sv_prepared_preset>& prepared, std::string& message)
    {
        if (path == nullptr || path[0] == '\0')
        {
            message = "sv_prepare_preset: path is empty";
            return SV_ERR_ARG;
        }

        if (BdHasMilk2Extension(path))
        {
            message = kMilk2Message;
            return SV_ERR_PARSE;
        }

        std::vector<char> includeText(kBeatdropResourceTextBytes);
        std::vector<char> defaultWarpText(kBeatdropResourceTextBytes);
        std::vector<char> defaultCompText(kBeatdropResourceTextBytes);
        BdLoadShaderResources(ctx->resourcePath, includeText.data(), defaultWarpText.data(), defaultCompText.data());

        const std::wstring nativePath = BdToNativePath(BdUtf8ToWide(path));

        if (!BdFileExists(nativePath.c_str()))
        {
            message = std::string("preset not found: ") + path;
            return SV_ERR_IO;
        }

        if (!BdFileLooksLikePreset(nativePath))
        {
            message = kNotPresetMessage;
            return SV_ERR_PARSE;
        }

        std::unique_ptr<CState> state(new CState());
        BdHostSink hostSink;
        ParserCapabilities caps;
        caps.isAMD = false;
        caps.maxPSVersion = MD2_PS_3_0;

        if (!state->ImportPrepare(nativePath.c_str(), nullptr, STATE_ALL, caps, defaultWarpText.data(), defaultCompText.data(),
            hostSink))
        {
            message = std::string("preset not readable: ") + path;
            return SV_ERR_IO;
        }

        if (hostSink.ErrorCount() > 0)
        {
            message = hostSink.LastError();
            return SV_ERR_PARSE;
        }

        prepared.reset(new sv_prepared_preset());
        prepared->owner = ctx;
        prepared->ownerSerial = ctx->serial;
        prepared->presetVersion = BdReadPresetVersion(nativePath);
        prepared->presetName = reportPresetName ? BdWideToUtf8(state->m_szDesc) : std::string();
        prepared->textureDirectory = textureDirectory;
        prepared->committed = false;

        BdPrepareStage(ctx->translationCache, render_gl::PresetShaderStage::Warp, state->m_nWarpPSVersion,
            state->m_szWarpShadersText, prepared->warp);
        BdPrepareStage(ctx->translationCache, render_gl::PresetShaderStage::Composite, state->m_nCompPSVersion,
            state->m_szCompShadersText, prepared->comp);

        prepared->state = std::move(state);
        message.clear();
        return SV_OK;
    }

    void BdPublishPresetInfo(sv_context* ctx, int presetVersion, const std::string& fallbackReason)
    {
        BdPresetInfoSnapshot snapshot;
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.presetVersion = presetVersion;
        snapshot.warpPsVersion = ctx->state->m_nWarpPSVersion;
        snapshot.compPsVersion = ctx->state->m_nCompPSVersion;
        snapshot.warpShader = ctx->gl != nullptr && ctx->gl->warpShaderActive ? 1 : 0;
        snapshot.compShader = ctx->gl != nullptr && ctx->gl->compShaderActive ? 1 : 0;
        BdCopyString(snapshot.fallbackReason, sizeof(snapshot.fallbackReason), fallbackReason.c_str());

        std::lock_guard<std::mutex> lock(ctx->presetInfoMutex);
        ctx->presetInfo = snapshot;
    }

    void BdCommitPreparedPreset(sv_context* ctx, sv_prepared_preset& prepared, float blendSeconds)
    {
        ctx->hostSink.Clear();

        if (wcscmp(ctx->state->m_szDesc, INVALID_PRESET_DESC) == 0)
            blendSeconds = 0.0f;

        CState* displaced = ctx->oldState;
        ctx->oldState = ctx->state;
        ctx->state = prepared.state.release();
        prepared.committed = true;
        delete displaced;

        const FrameInputs frameInputs = BdMakeFrameInputs(ctx);
        const float now = static_cast<float>(ctx->timeModel.GetTime());

        ctx->state->ImportCommit(now, frameInputs);
        BdStartPresetBlend(ctx, now, blendSeconds);

        if (blendSeconds >= 0.001f)
            BdRandomizeBlendPattern(ctx);

        BdClearPresetRequestLatch(ctx);
        ctx->resumePending = true;

        ctx->presetVersion = prepared.presetVersion;
        ctx->presetName = prepared.presetName;
        std::string fallbackReason;
        ctx->lastError = BdGlPreparePresetShaders(ctx, prepared.textureDirectory, &prepared.warp, &prepared.comp, &fallbackReason);
        BdPublishPresetInfo(ctx, prepared.presetVersion, fallbackReason);
    }
}

static sv_status BdLoadPresetFromImpl(sv_context* ctx, const char* path, float blend_seconds, const std::string& textureDirectory,
    bool reportPresetName)
{
    if (ctx == nullptr || path == nullptr || path[0] == '\0')
    {
        if (ctx != nullptr)
            BdSetPresetError(ctx, "sv_load_preset: path is empty");
        return SV_ERR_ARG;
    }

    if (BdHasMilk2Extension(path))
    {
        BdSetPresetError(ctx, kMilk2Message);
        return SV_ERR_PARSE;
    }

    BdLoadShaderResources(ctx->resourcePath, ctx->shaderIncludeText, ctx->defaultWarpShaderText, ctx->defaultCompShaderText);

    const std::wstring presetPath = BdUtf8ToWide(path);
    const std::wstring nativePath = BdToNativePath(presetPath);

    if (!BdFileExists(nativePath.c_str()))
    {
        std::string message("preset not found: ");
        message += path;
        BdSetPresetError(ctx, message);
        return SV_ERR_IO;
    }

    if (!BdFileLooksLikePreset(nativePath))
    {
        BdSetPresetError(ctx, kNotPresetMessage);
        return SV_ERR_PARSE;
    }

    const float previousPresetStartTime = ctx->presetStartTime;
    const float previousNextPresetTime = ctx->nextPresetTime;
    const bool previousNextPresetTimeWasInitialized = ctx->nextPresetTimeWasInitialized;

    const float appliedBlendSeconds = BdApplyPresetImport(ctx, nativePath, blend_seconds);
    ctx->resumePending = true;

    if (ctx->hostSink.ErrorCount() > 0)
    {
        CState* failed = ctx->state;
        ctx->state = ctx->oldState;
        ctx->oldState = failed;
        ctx->state->m_bBlending = false;

        ctx->presetStartTime = previousPresetStartTime;
        ctx->nextPresetTime = previousNextPresetTime;
        ctx->nextPresetTimeWasInitialized = previousNextPresetTimeWasInitialized;

        BdSetPresetError(ctx, ctx->hostSink.LastError());
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_PARSE;
    }

    if (appliedBlendSeconds >= 0.001f)
        BdRandomizeBlendPattern(ctx);

    BdClearPresetRequestLatch(ctx);

    ctx->presetVersion = BdReadPresetVersion(nativePath);
    if (reportPresetName)
        ctx->presetName = BdWideToUtf8(ctx->state->m_szDesc);
    else
        ctx->presetName.clear();

    std::string fallbackReason;
    ctx->lastError = BdGlPreparePresetShaders(ctx, textureDirectory, nullptr, nullptr, &fallbackReason);
    BdPublishPresetInfo(ctx, ctx->presetVersion, fallbackReason);

    if (ctx->gl != nullptr && !ctx->gl->ready)
    {
        ctx->lastError = "GL pipeline unavailable, preset loaded but cannot render";
        return SV_ERR_GL;
    }

    return SV_OK;
}

static sv_status BdLoadPresetFrom(sv_context* ctx, const char* path, float blend_seconds, const std::string& textureDirectory,
    bool reportPresetName)
{
    const sv_status status = BdLoadPresetFromImpl(ctx, path, blend_seconds, textureDirectory, reportPresetName);

    if (status != SV_OK && status != SV_ERR_GL)
        BdSignalPresetLoadFailure(ctx);

    return status;
}

extern "C" sv_status sv_load_preset(sv_context* ctx, const char* path, float blend_seconds)
{
    if (path == nullptr)
        return BdLoadPresetFrom(ctx, path, blend_seconds, std::string(), true);

    return BdLoadPresetFrom(ctx, path, blend_seconds, BdPresetDirectory(path), true);
}

extern "C" sv_status sv_load_preset_memory(sv_context* ctx, const char* text, size_t len, float blend_seconds)
{
    if (ctx == nullptr || text == nullptr)
    {
        if (ctx != nullptr)
            BdSetPresetError(ctx, "sv_load_preset_memory: text is null");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    if (!BdLooksLikePresetText(text, len))
    {
        BdSetPresetError(ctx, kNotPresetMessage);
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_PARSE;
    }

    char tempName[64];
    snprintf(tempName, sizeof(tempName), "bd-preset-memory-%p.tmp", static_cast<const void*>(ctx));

    const std::string tempPath = BdTempDirectory(ctx->cachePath) + "/" + tempName;

    if (!BdWriteTempFile(tempPath, text, len))
    {
        BdSetPresetError(ctx, "sv_load_preset_memory: could not create temporary file");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_IO;
    }

    const sv_status status = BdLoadPresetFrom(ctx, tempPath.c_str(), blend_seconds, ctx->textureSearchPath, false);

    remove(tempPath.c_str());
    return status;
}

extern "C" sv_status sv_validate_preset(const char* path, char* error, size_t error_len)
{
    std::string message;
    const sv_status status = BdValidatePresetFile(path, message);

    BdCopyError(error, error_len, message);
    return status;
}

extern "C" sv_status sv_validate_preset_memory(const char* text, size_t len, char* error, size_t error_len)
{
    if (text == nullptr)
    {
        BdCopyError(error, error_len, "sv_validate_preset_memory: text is null");
        return SV_ERR_ARG;
    }

    if (!BdLooksLikePresetText(text, len))
    {
        BdCopyError(error, error_len, kNotPresetMessage);
        return SV_ERR_PARSE;
    }

    static std::atomic<unsigned int> s_tempCounter(0);
    const unsigned int tempIndex = s_tempCounter.fetch_add(1);

    char tempName[96];
    snprintf(tempName, sizeof(tempName), "bd-preset-validate-%p-%u.tmp", static_cast<const void*>(&s_tempCounter), tempIndex);

    const std::string tempPath = BdTempDirectory(std::string()) + "/" + tempName;

    if (!BdWriteTempFile(tempPath, text, len))
    {
        BdCopyError(error, error_len, "sv_validate_preset_memory: could not create temporary file");
        return SV_ERR_IO;
    }

    std::string message;
    const sv_status status = BdValidatePresetFile(tempPath.c_str(), message);

    remove(tempPath.c_str());
    BdCopyError(error, error_len, message);
    return status;
}

extern "C" sv_status sv_prepare_preset(sv_context* ctx, const char* path, sv_prepared_preset** out_prepared, char* error,
    size_t error_len)
{
    if (out_prepared != nullptr)
        *out_prepared = nullptr;

    if (ctx == nullptr || out_prepared == nullptr)
    {
        BdCopyError(error, error_len, "sv_prepare_preset: context or output is null");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    std::unique_ptr<sv_prepared_preset> prepared;
    std::string message;
    const sv_status status = BdPreparePresetFile(ctx, path, path == nullptr ? std::string() : BdPresetDirectory(path), true,
        prepared, message);

    BdCopyError(error, error_len, message);

    if (status == SV_OK)
        *out_prepared = prepared.release();
    else
        BdSignalPresetLoadFailure(ctx);

    return status;
}

extern "C" sv_status sv_prepare_preset_memory(sv_context* ctx, const char* text, size_t len, sv_prepared_preset** out_prepared,
    char* error, size_t error_len)
{
    if (out_prepared != nullptr)
        *out_prepared = nullptr;

    if (ctx == nullptr || out_prepared == nullptr || text == nullptr)
    {
        BdCopyError(error, error_len, "sv_prepare_preset_memory: context, text or output is null");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    if (!BdLooksLikePresetText(text, len))
    {
        BdCopyError(error, error_len, kNotPresetMessage);
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_PARSE;
    }

    static std::atomic<unsigned int> s_tempCounter(0);
    const unsigned int tempIndex = s_tempCounter.fetch_add(1);

    char tempName[96];
    snprintf(tempName, sizeof(tempName), "bd-preset-prepare-%p-%u.tmp", static_cast<const void*>(&s_tempCounter), tempIndex);

    const std::string tempPath = BdTempDirectory(ctx->cachePath) + "/" + tempName;

    if (!BdWriteTempFile(tempPath, text, len))
    {
        BdCopyError(error, error_len, "sv_prepare_preset_memory: could not create temporary file");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_IO;
    }

    std::unique_ptr<sv_prepared_preset> prepared;
    std::string message;
    const sv_status status = BdPreparePresetFile(ctx, tempPath.c_str(), ctx->textureSearchPath, false, prepared, message);

    remove(tempPath.c_str());
    BdCopyError(error, error_len, message);

    if (status == SV_OK)
        *out_prepared = prepared.release();
    else
        BdSignalPresetLoadFailure(ctx);

    return status;
}

extern "C" sv_status sv_commit_preset(sv_context* ctx, sv_prepared_preset* prepared, float blend_seconds)
{
    if (ctx == nullptr)
        return SV_ERR_ARG;

    if (prepared == nullptr)
    {
        BdSetPresetError(ctx, "sv_commit_preset: prepared preset is null");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    if (prepared->committed)
    {
        BdSetPresetError(ctx, "sv_commit_preset: prepared preset was already committed");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    if (prepared->owner != ctx || prepared->ownerSerial != ctx->serial)
    {
        BdSetPresetError(ctx, "sv_commit_preset: prepared preset belongs to another context");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_ARG;
    }

    if (ctx->gl != nullptr && !ctx->gl->ready)
    {
        BdSetPresetError(ctx, "GL pipeline unavailable, preset not committed");
        BdSignalPresetLoadFailure(ctx);
        return SV_ERR_GL;
    }

    BdCommitPreparedPreset(ctx, *prepared, blend_seconds);
    return SV_OK;
}

extern "C" void sv_prepared_preset_free(sv_prepared_preset* prepared)
{
    delete prepared;
}

extern "C" sv_status sv_get_preset_info(sv_context* ctx, sv_preset_info* out)
{
    if (ctx == nullptr || out == nullptr)
        return SV_ERR_ARG;

    BdPresetInfoSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(ctx->presetInfoMutex);
        snapshot = ctx->presetInfo;
    }

    out->preset_version = snapshot.presetVersion;
    out->warp_ps_version = snapshot.warpPsVersion;
    out->comp_ps_version = snapshot.compPsVersion;
    out->warp_shader = snapshot.warpShader;
    out->comp_shader = snapshot.compShader;
    BdCopyString(out->fallback_reason, sizeof(out->fallback_reason), snapshot.fallbackReason);
    return SV_OK;
}
