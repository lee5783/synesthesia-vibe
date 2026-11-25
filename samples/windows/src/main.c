//
//  main.c
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <synesthesia-vibe/synesthesia-vibe-windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

enum
{
    kMaxPresets = 256,
    kPathChars = 1024,
    kUtf8PathBytes = 4096,
    kTitleChars = 512,
    kErrorBytes = 1024,
    kDefaultWidth = 1280,
    kDefaultHeight = 720,
    kCaptureWidth = 848,
    kCaptureHeight = 448,
    kDefaultCaptureFrames = 120,
    kParentSearchDepth = 8,
    kCycleTimerId = 1,
    kPauseTimerId = 2,
    kExitTimerId = 3,
    kSnapshotTimerId = 4,
    kLevelsTimerId = 5,
    kGroupToggleTimerId = 6,
    kCloseMemberTimerId = 7,
    kAddMemberTimerId = 8,
    kSwitchSourceTimerId = 9,
    kKillMemberTimerId = 10,
    kReapMemberTimerId = 11,
    kCapSteps = 3,
    kMaxSyncMembers = 8,
    kSyncDefaultWidth = 640,
    kSyncDefaultHeight = 360,
    kDeviceNameBytes = 256
};

static const float kBlendSeconds = 2.0f;
static const double kCaptureFrameSeconds = 1.0 / 60.0;
static const uint32_t kCaptureRngSeed = 1;
static const float kDebugIntervalSeconds = 1.0f;
static const float kScaleSteps[] = { 0.25f, 0.5f, 0.75f, 1.0f };
static const float kCapSequence[kCapSteps] = { 30.0f, 60.0f, 0.0f };
static const wchar_t* kWindowClass = L"SynesthesiaVibeWindowsSample";
static const wchar_t* kSyncWindowClass = L"SynesthesiaVibeWindowsSyncSample";
static const UINT kReconcileMessage = WM_APP + 1;

typedef struct SampleOptions
{
    wchar_t presetsDir[kPathChars];
    wchar_t resourcesDir[kPathChars];
    wchar_t captureDir[kPathChars];
    wchar_t snapshotDir[kPathChars];
    wchar_t presetFilter[kPathChars];
    int width;
    int height;
    int frames;
    int adapter;
    float maxFps;
    float renderScale;
    float maxLuminance;
    float flashLimit;
    double cycleSeconds;
    double pauseAfterSeconds;
    double exitAfterSeconds;
    double snapshotAfterSeconds;
    double ungroupAfterSeconds;
    double closeMemberAfterSeconds;
    double addMemberAfterSeconds;
    double switchSourceAfterSeconds;
    double killMemberAfterSeconds;
    int secondWidth;
    int secondHeight;
    int scaling;
    float gain;
    bool fpsLog;
    bool sync;
    bool audio;
    bool microphone;
    bool levelsLog;
} SampleOptions;

typedef struct SampleState
{
    SampleOptions* options;
    HWND hwnd;
    svw_view* view;
    wchar_t presets[kMaxPresets][kPathChars];
    int presetCount;
    int presetIndex;
    bool paused;
    float renderScale;
    float maxFps;
    int snapshotCount;
} SampleState;

typedef struct CaptureJob
{
    svw_view* view;
    char presetPath[kUtf8PathBytes];
    char pngPath[kUtf8PathBytes];
    int frames;
    sv_status status;
    char error[kErrorBytes];
} CaptureJob;

typedef struct SyncMember
{
    HWND hwnd;
    svw_view* view;
    wchar_t monitor[CCHDEVICENAME];
} SyncMember;

typedef struct SyncState
{
    SampleState* sample;
    HWND controller;
    svw_group* group;
    SyncMember members[kMaxSyncMembers];
    int memberCount;
    svw_audio_input* input;
    int audioSource;
    float gain;
    float levels[3];
    int levelTicks;
    int scaling;
    bool grouped;
    bool paused;
    bool reconciling;
    char lastDevice[kDeviceNameBytes];
    char resources[kUtf8PathBytes];
    sv_config engine;
} SyncState;

static SampleState g_state;
static SyncState g_sync;

static void ToUtf8(const wchar_t* text, char* out, int outBytes)
{
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, out, outBytes, NULL, NULL) == 0)
        out[0] = '\0';
}

static bool DirectoryExists(const wchar_t* path)
{
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static const wchar_t* BaseName(const wchar_t* path)
{
    const wchar_t* slash = wcsrchr(path, L'\\');
    return slash != NULL ? slash + 1 : path;
}

static void PresetDisplayName(const wchar_t* path, wchar_t* out, size_t outChars)
{
    wcsncpy_s(out, outChars, BaseName(path), _TRUNCATE);
    wchar_t* extension = wcsrchr(out, L'.');
    if (extension != NULL)
        *extension = L'\0';
}

static void PresetSlug(const wchar_t* path, wchar_t* out, size_t outChars)
{
    wchar_t name[kPathChars];
    PresetDisplayName(path, name, kPathChars);

    size_t length = 0;
    bool pendingHyphen = false;
    for (const wchar_t* c = name; *c != L'\0' && length + 2 < outChars; ++c)
    {
        if (iswalnum(*c) && *c < 128)
        {
            if (pendingHyphen && length > 0)
                out[length++] = L'-';
            out[length++] = (wchar_t)towlower(*c);
            pendingHyphen = false;
        }
        else
        {
            pendingHyphen = true;
        }
    }
    out[length] = L'\0';
}

static bool FindDefaultResources(SampleOptions* options)
{
    wchar_t directory[kPathChars];
    if (GetModuleFileNameW(NULL, directory, kPathChars) == 0)
        return false;

    for (int depth = 0; depth < kParentSearchDepth; ++depth)
    {
        wchar_t* slash = wcsrchr(directory, L'\\');
        if (slash == NULL)
            return false;
        *slash = L'\0';

        wchar_t presets[kPathChars];
        swprintf(presets, kPathChars, L"%ls\\resources\\mvp-presets", directory);
        if (DirectoryExists(presets))
        {
            if (options->presetsDir[0] == L'\0')
                wcsncpy_s(options->presetsDir, kPathChars, presets, _TRUNCATE);
            if (options->resourcesDir[0] == L'\0')
                swprintf(options->resourcesDir, kPathChars, L"%ls\\resources\\beatdrop", directory);
            return true;
        }
    }

    return false;
}

static int ComparePaths(const void* left, const void* right)
{
    return _wcsicmp((const wchar_t*)left, (const wchar_t*)right);
}

static bool MatchesFilter(const wchar_t* path, const wchar_t* filter)
{
    if (filter[0] == L'\0')
        return true;

    wchar_t name[kPathChars];
    wchar_t needle[kPathChars];
    wcsncpy_s(name, kPathChars, BaseName(path), _TRUNCATE);
    wcsncpy_s(needle, kPathChars, filter, _TRUNCATE);
    _wcslwr_s(name, kPathChars);
    _wcslwr_s(needle, kPathChars);
    return wcsstr(name, needle) != NULL;
}

static int ListPresets(SampleState* state)
{
    wchar_t pattern[kPathChars];
    swprintf(pattern, kPathChars, L"%ls\\*.milk", state->options->presetsDir);

    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || state->presetCount >= kMaxPresets)
            continue;

        wchar_t* slot = state->presets[state->presetCount];
        swprintf(slot, kPathChars, L"%ls\\%ls", state->options->presetsDir, entry.cFileName);
        if (MatchesFilter(slot, state->options->presetFilter))
            ++state->presetCount;
    } while (FindNextFileW(search, &entry));

    FindClose(search);
    qsort(state->presets, (size_t)state->presetCount, sizeof(state->presets[0]), ComparePaths);
    return state->presetCount;
}

static void PrintError(const char* what, svw_view* view)
{
    fprintf(stderr, "%s failed: %s\n", what, svw_last_error(view));
    fflush(stderr);
}

static void UpdateTitle(SampleState* state)
{
    wchar_t name[kPathChars] = L"";
    if (state->presetCount > 0)
        PresetDisplayName(state->presets[state->presetIndex], name, kPathChars);

    wchar_t cap[32];
    if (state->maxFps > 0.0f)
        swprintf(cap, 32, L"%.0f fps", state->maxFps);
    else
        swprintf(cap, 32, L"refresh");

    wchar_t title[kTitleChars];
    swprintf(title, kTitleChars, L"SynesthesiaVibe - %ls | scale %.2f | cap %ls%ls",
             name, state->renderScale, cap, state->paused ? L" | paused" : L"");
    SetWindowTextW(state->hwnd, title);
}

static void LoadCurrentPreset(SampleState* state, float blendSeconds)
{
    if (state->presetCount == 0)
        return;

    char path[kUtf8PathBytes];
    ToUtf8(state->presets[state->presetIndex], path, kUtf8PathBytes);
    if (svw_view_load_preset(state->view, path, blendSeconds) != SV_OK)
        PrintError("svw_view_load_preset", state->view);
    else
        printf("PRESET %s\n", path);
    fflush(stdout);
    UpdateTitle(state);
}

static void StepPreset(SampleState* state, int step)
{
    if (state->presetCount == 0)
        return;

    state->presetIndex = (state->presetIndex + step + state->presetCount) % state->presetCount;
    LoadCurrentPreset(state, kBlendSeconds);
}

static void SetPaused(SampleState* state, bool paused)
{
    state->paused = paused;
    const sv_status status = paused ? svw_view_pause(state->view) : svw_view_resume(state->view);
    if (status != SV_OK)
        PrintError(paused ? "svw_view_pause" : "svw_view_resume", state->view);
    printf("%s\n", paused ? "PAUSED" : "RESUMED");
    fflush(stdout);
    UpdateTitle(state);
}

static void SetScale(SampleState* state, float scale)
{
    state->renderScale = scale;
    if (svw_view_set_render_scale(state->view, scale) != SV_OK)
        PrintError("svw_view_set_render_scale", state->view);
    printf("SCALE %.2f\n", scale);
    fflush(stdout);
    UpdateTitle(state);
}

static void CycleCap(SampleState* state)
{
    int next = 0;
    for (int index = 0; index < kCapSteps; ++index)
    {
        if (kCapSequence[index] == state->maxFps)
            next = (index + 1) % kCapSteps;
    }

    state->maxFps = kCapSequence[next];
    if (svw_view_set_preferred_fps(state->view, state->maxFps) != SV_OK)
        PrintError("svw_view_set_preferred_fps", state->view);
    printf("CAP %.0f\n", state->maxFps);
    fflush(stdout);
    UpdateTitle(state);
}

static void TakeSnapshot(SampleState* state)
{
    wchar_t path[kPathChars];
    const wchar_t* directory = state->options->snapshotDir[0] != L'\0' ? state->options->snapshotDir : L".";
    swprintf(path, kPathChars, L"%ls\\snapshot-%03d.png", directory, ++state->snapshotCount);

    char pathUtf8[kUtf8PathBytes];
    ToUtf8(path, pathUtf8, kUtf8PathBytes);
    if (svw_view_snapshot_png(state->view, pathUtf8) != SV_OK)
        PrintError("svw_view_snapshot_png", state->view);
    else
        printf("SNAPSHOT %s\n", pathUtf8);
    fflush(stdout);
}

static void PrintDebugInfo(const sv_debug_info* info, void* user)
{
    (void)user;
    printf("FPS display=%.1f steps=%.2f size=%dx%d preset=%s\n",
           info->display_fps, info->sim_steps_per_frame, info->width, info->height, info->preset_name);
    fflush(stdout);
}

static void EnableDebugLog(sv_context* ctx, uint32_t target_fbo, void* user)
{
    (void)target_fbo;
    (void)user;
    sv_set_debug_interval(ctx, kDebugIntervalSeconds);
    sv_set_debug_callback(ctx, PrintDebugInfo, NULL);
}

static void ArmTimer(HWND hwnd, UINT_PTR id, double seconds)
{
    if (seconds > 0.0)
        SetTimer(hwnd, id, (UINT)(seconds * 1000.0), NULL);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SampleState* state = &g_state;

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        ValidateRect(hwnd, NULL);
        return 0;

    case WM_DPICHANGED:
    {
        const RECT* suggested = (const RECT*)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_KEYDOWN:
        if (state->view == NULL)
            break;

        switch (wParam)
        {
        case VK_SPACE:
            SetPaused(state, !state->paused);
            return 0;
        case VK_RIGHT:
        case 'N':
            StepPreset(state, 1);
            return 0;
        case VK_LEFT:
        case 'P':
            StepPreset(state, -1);
            return 0;
        case '1':
        case '2':
        case '3':
        case '4':
            SetScale(state, kScaleSteps[wParam - '1']);
            return 0;
        case 'C':
            CycleCap(state);
            return 0;
        case 'S':
            TakeSnapshot(state);
            return 0;
        case VK_ESCAPE:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        default:
            break;
        }
        break;

    case WM_TIMER:
        if (state->view == NULL)
            break;

        if (wParam == kCycleTimerId)
        {
            StepPreset(state, 1);
        }
        else if (wParam == kPauseTimerId)
        {
            KillTimer(hwnd, kPauseTimerId);
            KillTimer(hwnd, kCycleTimerId);
            SetPaused(state, true);
        }
        else if (wParam == kSnapshotTimerId)
        {
            KillTimer(hwnd, kSnapshotTimerId);
            TakeSnapshot(state);
        }
        else if (wParam == kExitTimerId)
        {
            KillTimer(hwnd, kExitTimerId);
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;

    case WM_CLOSE:
        if (state->view != NULL)
        {
            svw_view_destroy(state->view);
            state->view = NULL;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (state->hwnd == hwnd)
            PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HWND CreateSampleWindow(int clientWidth, int clientHeight)
{
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRectExForDpi(&rect, style, FALSE, 0, GetDpiForSystem());

    HWND hwnd = CreateWindowExW(0, kWindowClass, L"SynesthesiaVibe", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (hwnd != NULL)
    {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

static void PumpPendingMessages(void)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

static sv_config EngineConfig(const SampleOptions* options, const char* resourcesUtf8)
{
    sv_config engine;
    memset(&engine, 0, sizeof(engine));
    engine.resource_path = resourcesUtf8;
    engine.max_fps = options->maxFps;
    engine.max_luminance = options->maxLuminance;
    engine.flash_limit = options->flashLimit;
    return engine;
}

static void CaptureFrames(sv_context* ctx, uint32_t target_fbo, void* user)
{
    CaptureJob* job = (CaptureJob*)user;

    sv_resume(ctx);
    job->status = sv_load_preset(ctx, job->presetPath, 0.0f);
    if (job->status != SV_OK)
    {
        snprintf(job->error, kErrorBytes, "%s", sv_last_error(ctx));
        sv_pause(ctx);
        return;
    }

    for (int frame = 0; frame < job->frames; ++frame)
        sv_render_frame(ctx, kCaptureFrameSeconds, target_fbo);

    job->status = svw_view_snapshot_png(job->view, job->pngPath);
    if (job->status != SV_OK)
        snprintf(job->error, kErrorBytes, "%s", svw_last_error(job->view));

    sv_pause(ctx);
}

static void CreateDirectoryTree(const wchar_t* directory)
{
    wchar_t fullPath[kPathChars];
    const DWORD length = GetFullPathNameW(directory, kPathChars, fullPath, NULL);
    if (length == 0 || length >= kPathChars)
        return;

    SHCreateDirectoryExW(NULL, fullPath, NULL);
}

static int RunCapture(SampleState* state)
{
    const SampleOptions* options = state->options;
    CreateDirectoryTree(options->captureDir);

    char resourcesUtf8[kUtf8PathBytes];
    ToUtf8(options->resourcesDir, resourcesUtf8, kUtf8PathBytes);

    int failures = 0;
    for (int index = 0; index < state->presetCount; ++index)
    {
        HWND hwnd = CreateSampleWindow(options->width, options->height);
        if (hwnd == NULL)
        {
            fprintf(stderr, "CreateWindowExW failed (%lu)\n", GetLastError());
            return 1;
        }
        PumpPendingMessages();

        wchar_t slug[kPathChars];
        wchar_t pngPath[kPathChars];
        PresetSlug(state->presets[index], slug, kPathChars);
        swprintf(pngPath, kPathChars, L"%ls\\%ls.png", options->captureDir, slug);

        CaptureJob job;
        memset(&job, 0, sizeof(job));
        job.frames = options->frames;
        job.status = SV_ERR_GL;
        ToUtf8(state->presets[index], job.presetPath, kUtf8PathBytes);
        ToUtf8(pngPath, job.pngPath, kUtf8PathBytes);

        sv_config engine = EngineConfig(options, resourcesUtf8);
        engine.deterministic = true;
        engine.rng_seed = kCaptureRngSeed;
        engine.fps_hint = (float)(1.0 / kCaptureFrameSeconds);
        engine.pin_date = true;
        engine.year = 2000.0f;
        engine.month = 1.0f;
        engine.day = 1.0f;
        engine.weekday = 6.0f;

        svw_view_config config;
        memset(&config, 0, sizeof(config));
        config.hwnd = hwnd;
        config.render_scale = 1.0f;
        config.adapter = options->adapter;
        config.start_paused = true;

        if (svw_view_create(&config, &engine, &job.view) != SV_OK)
        {
            snprintf(job.error, kErrorBytes, "%s", svw_last_error(NULL));
        }
        else
        {
            if (svw_view_invoke(job.view, CaptureFrames, &job) != SV_OK)
                snprintf(job.error, kErrorBytes, "%s", svw_last_error(job.view));
            svw_view_destroy(job.view);
        }

        if (job.status == SV_OK)
        {
            printf("CAPTURE OK %s -> %s\n", job.presetPath, job.pngPath);
        }
        else
        {
            printf("CAPTURE FAIL %s (status %d): %s\n", job.presetPath, (int)job.status, job.error);
            ++failures;
        }
        fflush(stdout);

        DestroyWindow(hwnd);
        PumpPendingMessages();
    }

    printf("CAPTURE DONE %d/%d ok\n", state->presetCount - failures, state->presetCount);
    return failures == 0 ? 0 : 1;
}

static int RunInteractive(SampleState* state)
{
    const SampleOptions* options = state->options;

    state->hwnd = CreateSampleWindow(options->width, options->height);
    if (state->hwnd == NULL)
    {
        fprintf(stderr, "CreateWindowExW failed (%lu)\n", GetLastError());
        return 1;
    }

    char resourcesUtf8[kUtf8PathBytes];
    ToUtf8(options->resourcesDir, resourcesUtf8, kUtf8PathBytes);
    sv_config engine = EngineConfig(options, resourcesUtf8);

    svw_view_config config;
    memset(&config, 0, sizeof(config));
    config.hwnd = state->hwnd;
    config.render_scale = options->renderScale;
    config.adapter = options->adapter;

    svw_view* view = NULL;
    if (svw_view_create(&config, &engine, &view) != SV_OK)
    {
        PrintError("svw_view_create", NULL);
        DestroyWindow(state->hwnd);
        return 1;
    }

    state->view = view;
    state->renderScale = options->renderScale;
    state->maxFps = options->maxFps;

    if (options->fpsLog && svw_view_invoke(view, EnableDebugLog, NULL) != SV_OK)
        PrintError("svw_view_invoke", view);

    LoadCurrentPreset(state, 0.0f);

    ArmTimer(state->hwnd, kCycleTimerId, options->cycleSeconds);
    ArmTimer(state->hwnd, kPauseTimerId, options->pauseAfterSeconds);
    ArmTimer(state->hwnd, kSnapshotTimerId, options->snapshotAfterSeconds);
    ArmTimer(state->hwnd, kExitTimerId, options->exitAfterSeconds);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}

static const char* ScalingName(int scaling)
{
    return scaling == SVW_SYNC_SCALING_FIT ? "fit" : scaling == SVW_SYNC_SCALING_STRETCH ? "stretch" : "fill";
}

static const char* SourceName(int source)
{
    return source == SVW_AUDIO_MICROPHONE ? "microphone" : "loopback";
}

static void PrintGroupSize(SyncState* sync)
{
    int width = 0;
    int height = 0;
    int members = 0;
    if (svw_group_get_render_size(sync->group, &width, &height) != SV_OK)
        fprintf(stderr, "svw_group_get_render_size failed: %s\n", svw_group_last_error(sync->group));
    svw_group_get_member_count(sync->group, &members);
    printf("GROUP size=%dx%d members=%d grouped=%d\n", width, height, members, sync->grouped ? 1 : 0);
    fflush(stdout);
}

static void UpdateSyncTitles(SyncState* sync)
{
    SampleState* state = sync->sample;
    wchar_t name[kPathChars] = L"";
    if (state->presetCount > 0)
        PresetDisplayName(state->presets[state->presetIndex], name, kPathChars);

    for (int index = 0; index < sync->memberCount; ++index)
    {
        wchar_t title[kTitleChars];
        swprintf(title, kTitleChars, L"SynesthesiaVibe sync %d - %ls | %hs | %hs | gain %.2f | %hs | bass %.2f mid %.2f treb %.2f%ls",
                 index, name, sync->grouped ? ScalingName(sync->scaling) : "own", sync->grouped ? "grouped" : "ungrouped", sync->gain,
                 sync->input != NULL ? SourceName(sync->audioSource) : "no audio", sync->levels[0], sync->levels[1], sync->levels[2],
                 sync->paused ? L" | paused" : L"");
        SetWindowTextW(sync->members[index].hwnd, title);
    }
}

static void LoadSyncPreset(SyncState* sync, float blendSeconds)
{
    SampleState* state = sync->sample;
    if (state->presetCount == 0)
        return;

    char path[kUtf8PathBytes];
    ToUtf8(state->presets[state->presetIndex], path, kUtf8PathBytes);
    if (sync->grouped)
    {
        if (svw_group_load_preset(sync->group, path, blendSeconds) != SV_OK)
            fprintf(stderr, "svw_group_load_preset failed: %s\n", svw_group_last_error(sync->group));
    }
    else
    {
        for (int index = 0; index < sync->memberCount; ++index)
        {
            if (svw_view_load_preset(sync->members[index].view, path, blendSeconds) != SV_OK)
                PrintError("svw_view_load_preset", sync->members[index].view);
        }
    }
    printf("PRESET %s\n", path);
    fflush(stdout);
    UpdateSyncTitles(sync);
}

static void StepSyncPreset(SyncState* sync, int step)
{
    SampleState* state = sync->sample;
    if (state->presetCount == 0)
        return;

    state->presetIndex = (state->presetIndex + step + state->presetCount) % state->presetCount;
    LoadSyncPreset(sync, kBlendSeconds);
}

static bool StartAudio(SyncState* sync, int source)
{
    if (sync->input != NULL)
    {
        svw_audio_input_destroy(sync->input);
        sync->input = NULL;
    }

    svw_audio_input* input = NULL;
    if (svw_audio_input_create(source, &input) != SV_OK)
    {
        fprintf(stderr, "svw_audio_input_create failed: %s\n", svw_last_error(NULL));
        return false;
    }

    sync->audioSource = source;
    svw_audio_input_set_gain(input, sync->gain);
    if (svw_audio_input_start(input) != SV_OK)
    {
        printf("AUDIO FAIL source=%s %s\n", SourceName(source), svw_audio_input_last_error(input));
        fflush(stdout);
        svw_audio_input_destroy(input);
        return false;
    }

    sync->input = input;
    sync->lastDevice[0] = '\0';
    printf("AUDIO START source=%s device=%s\n", SourceName(source), svw_audio_input_device_name(input));
    fflush(stdout);
    return true;
}

static void SetSyncGain(SyncState* sync, float gain)
{
    sync->gain = gain < 0.0f ? 0.0f : gain > 4.0f ? 4.0f : gain;
    if (sync->input != NULL && svw_audio_input_set_gain(sync->input, sync->gain) != SV_OK)
        fprintf(stderr, "svw_audio_input_set_gain failed: %s\n", svw_audio_input_last_error(sync->input));
    printf("GAIN %.2f\n", sync->gain);
    fflush(stdout);
    UpdateSyncTitles(sync);
}

static void PollLevels(SyncState* sync, bool log)
{
    if (sync->input == NULL)
        return;

    svw_audio_input_get_levels(sync->input, &sync->levels[0], &sync->levels[1], &sync->levels[2]);

    const char* device = svw_audio_input_device_name(sync->input);
    if (strcmp(device, sync->lastDevice) != 0)
    {
        if (sync->lastDevice[0] != '\0')
        {
            printf("AUDIO DEVICE %s (%s)\n", device, svw_audio_input_last_error(sync->input));
            fflush(stdout);
        }
        strncpy_s(sync->lastDevice, sizeof(sync->lastDevice), device, _TRUNCATE);
    }

    if (log)
    {
        printf("LEVELS bass=%.3f mid=%.3f treb=%.3f gain=%.2f source=%s device=%s\n", sync->levels[0], sync->levels[1], sync->levels[2],
               sync->gain, SourceName(sync->audioSource), device);
        fflush(stdout);
    }
}

static void JoinOrLeave(SyncState* sync, int index)
{
    svw_view* view = sync->members[index].view;
    if (svw_view_set_sync_group(view, sync->grouped ? sync->group : NULL) != SV_OK)
        PrintError("svw_view_set_sync_group", view);
}

static int AddSyncMember(SyncState* sync, const wchar_t* monitor, int x, int y, int width, int height)
{
    if (sync->memberCount >= kMaxSyncMembers)
        return -1;

    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = { 0, 0, width, height };
    AdjustWindowRectExForDpi(&rect, style, FALSE, 0, GetDpiForSystem());
    HWND hwnd = CreateWindowExW(0, kSyncWindowClass, L"SynesthesiaVibe sync", style, x, y, rect.right - rect.left, rect.bottom - rect.top,
                                NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (hwnd == NULL)
    {
        fprintf(stderr, "CreateWindowExW failed (%lu)\n", GetLastError());
        return -1;
    }
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    svw_view_config config;
    memset(&config, 0, sizeof(config));
    config.hwnd = hwnd;
    config.render_scale = 1.0f;
    config.adapter = sync->sample->options->adapter;

    svw_view* view = NULL;
    if (svw_view_create(&config, &sync->engine, &view) != SV_OK)
    {
        PrintError("svw_view_create", NULL);
        DestroyWindow(hwnd);
        return -1;
    }

    const int index = sync->memberCount++;
    SyncMember* member = &sync->members[index];
    member->hwnd = hwnd;
    member->view = view;
    wcsncpy_s(member->monitor, CCHDEVICENAME, monitor, _TRUNCATE);
    svw_view_set_sync_scaling(view, sync->scaling);
    JoinOrLeave(sync, index);
    if (!sync->grouped && sync->sample->presetCount > 0)
    {
        char path[kUtf8PathBytes];
        ToUtf8(sync->sample->presets[sync->sample->presetIndex], path, kUtf8PathBytes);
        svw_view_load_preset(view, path, 0.0f);
    }

    char monitorUtf8[64];
    ToUtf8(monitor, monitorUtf8, sizeof(monitorUtf8));
    printf("JOIN member=%d monitor=%s size=%dx%d\n", index, monitorUtf8, width, height);
    fflush(stdout);
    PrintGroupSize(sync);
    UpdateSyncTitles(sync);
    return index;
}

static void CloseSyncMember(SyncState* sync, int index, const char* reason)
{
    if (index < 0 || index >= sync->memberCount)
        return;

    SyncMember member = sync->members[index];
    for (int next = index; next + 1 < sync->memberCount; ++next)
        sync->members[next] = sync->members[next + 1];
    --sync->memberCount;

    svw_view_destroy(member.view);
    DestroyWindow(member.hwnd);
    printf("LEAVE member=%d reason=%s\n", index, reason);
    fflush(stdout);
    PrintGroupSize(sync);

    if (sync->memberCount == 0 && !sync->reconciling)
        PostQuitMessage(0);
}

typedef struct MonitorList
{
    int count;
    wchar_t names[kMaxSyncMembers][CCHDEVICENAME];
    RECT work[kMaxSyncMembers];
} MonitorList;

static BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM user)
{
    (void)dc;
    (void)rect;
    MonitorList* list = (MonitorList*)user;
    MONITORINFOEXW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (list->count < kMaxSyncMembers && GetMonitorInfoW(monitor, (MONITORINFO*)&info))
    {
        wcsncpy_s(list->names[list->count], CCHDEVICENAME, info.szDevice, _TRUNCATE);
        list->work[list->count] = info.rcWork;
        ++list->count;
    }
    return TRUE;
}

static int MembersOnMonitor(SyncState* sync, const wchar_t* monitor)
{
    int count = 0;
    for (int index = 0; index < sync->memberCount; ++index)
    {
        if (wcscmp(sync->members[index].monitor, monitor) == 0)
            ++count;
    }
    return count;
}

static void ReconcileMembers(SyncState* sync)
{
    MonitorList monitors;
    memset(&monitors, 0, sizeof(monitors));
    EnumDisplayMonitors(NULL, NULL, CollectMonitor, (LPARAM)&monitors);
    if (monitors.count == 0)
        return;

    sync->reconciling = true;
    for (int index = sync->memberCount - 1; index >= 0; --index)
    {
        bool present = false;
        for (int monitor = 0; monitor < monitors.count; ++monitor)
            present = present || wcscmp(sync->members[index].monitor, monitors.names[monitor]) == 0;
        if (!present)
            CloseSyncMember(sync, index, "monitor-removed");
    }

    const SampleOptions* options = sync->sample->options;
    const int perMonitor = monitors.count >= 2 ? 1 : 2;
    for (int monitor = 0; monitor < monitors.count; ++monitor)
    {
        const RECT work = monitors.work[monitor];
        for (int slot = MembersOnMonitor(sync, monitors.names[monitor]); slot < perMonitor; ++slot)
        {
            const bool second = slot == 1 && options->secondWidth > 0;
            const int width = second ? options->secondWidth : options->width;
            const int height = second ? options->secondHeight : options->height;
            const int x = work.left + 16 + slot * ((work.right - work.left) / 2);
            AddSyncMember(sync, monitors.names[monitor], x, work.top + 16, width, height);
        }
    }
    sync->reconciling = false;

    if (sync->memberCount == 0)
        PostQuitMessage(0);
}

static void ToggleGrouped(SyncState* sync)
{
    sync->grouped = !sync->grouped;
    for (int index = 0; index < sync->memberCount; ++index)
        JoinOrLeave(sync, index);
    printf("%s\n", sync->grouped ? "GROUPED" : "UNGROUPED");
    fflush(stdout);
    LoadSyncPreset(sync, 0.0f);
    PrintGroupSize(sync);
}

static void SetSyncPaused(SyncState* sync, bool paused)
{
    sync->paused = paused;
    if (paused)
        svw_group_pause(sync->group);
    else
        svw_group_resume(sync->group);
    for (int index = 0; index < sync->memberCount; ++index)
    {
        if (paused)
            svw_view_pause(sync->members[index].view);
        else
            svw_view_resume(sync->members[index].view);
    }
    printf("%s\n", paused ? "PAUSED" : "RESUMED");
    fflush(stdout);
    UpdateSyncTitles(sync);
}

static void SnapshotMembers(SyncState* sync)
{
    const wchar_t* directory = sync->sample->options->snapshotDir[0] != L'\0' ? sync->sample->options->snapshotDir : L".";
    const bool hold = sync->grouped && !sync->paused;
    if (hold)
        svw_group_pause(sync->group);

    ++sync->sample->snapshotCount;
    for (int index = 0; index < sync->memberCount; ++index)
    {
        wchar_t path[kPathChars];
        swprintf(path, kPathChars, L"%ls\\sync-%03d-%hs-m%d.png", directory, sync->sample->snapshotCount,
                 sync->grouped ? ScalingName(sync->scaling) : "ungrouped", index);
        char pathUtf8[kUtf8PathBytes];
        ToUtf8(path, pathUtf8, kUtf8PathBytes);
        if (svw_view_snapshot_png(sync->members[index].view, pathUtf8) != SV_OK)
            PrintError("svw_view_snapshot_png", sync->members[index].view);
        else
            printf("SNAPSHOT member=%d %s\n", index, pathUtf8);
    }
    fflush(stdout);

    if (hold)
        svw_group_resume(sync->group);
}

static void CycleSyncCap(SyncState* sync)
{
    int next = 0;
    for (int index = 0; index < kCapSteps; ++index)
    {
        if (kCapSequence[index] == sync->sample->maxFps)
            next = (index + 1) % kCapSteps;
    }

    sync->sample->maxFps = kCapSequence[next];
    svw_group_set_preferred_fps(sync->group, sync->sample->maxFps);
    for (int index = 0; index < sync->memberCount; ++index)
        svw_view_set_preferred_fps(sync->members[index].view, sync->sample->maxFps);
    printf("CAP %.0f\n", sync->sample->maxFps);
    fflush(stdout);
}

static int FindSyncMember(SyncState* sync, HWND hwnd)
{
    for (int index = 0; index < sync->memberCount; ++index)
    {
        if (sync->members[index].hwnd == hwnd)
            return index;
    }
    return -1;
}

static bool HandleSyncKey(SyncState* sync, WPARAM key)
{
    switch (key)
    {
    case VK_SPACE:
        SetSyncPaused(sync, !sync->paused);
        return true;
    case VK_RIGHT:
    case 'N':
        StepSyncPreset(sync, 1);
        return true;
    case VK_LEFT:
    case 'P':
        StepSyncPreset(sync, -1);
        return true;
    case 'F':
        sync->scaling = (sync->scaling + 1) % 3;
        for (int index = 0; index < sync->memberCount; ++index)
            svw_view_set_sync_scaling(sync->members[index].view, sync->scaling);
        printf("SCALING %s\n", ScalingName(sync->scaling));
        fflush(stdout);
        UpdateSyncTitles(sync);
        return true;
    case 'G':
        ToggleGrouped(sync);
        return true;
    case VK_UP:
        SetSyncGain(sync, sync->gain + 0.25f);
        return true;
    case VK_DOWN:
        SetSyncGain(sync, sync->gain - 0.25f);
        return true;
    case 'M':
        StartAudio(sync, sync->audioSource == SVW_AUDIO_LOOPBACK ? SVW_AUDIO_MICROPHONE : SVW_AUDIO_LOOPBACK);
        UpdateSyncTitles(sync);
        return true;
    case '1':
    case '2':
    case '3':
    case '4':
        svw_group_set_render_scale(sync->group, kScaleSteps[key - '1']);
        for (int index = 0; index < sync->memberCount; ++index)
            svw_view_set_render_scale(sync->members[index].view, kScaleSteps[key - '1']);
        printf("SCALE %.2f\n", kScaleSteps[key - '1']);
        PrintGroupSize(sync);
        return true;
    case 'C':
        CycleSyncCap(sync);
        return true;
    case 'S':
        SnapshotMembers(sync);
        return true;
    case 'A':
        if (sync->memberCount > 0)
        {
            RECT rect;
            GetWindowRect(sync->members[0].hwnd, &rect);
            AddSyncMember(sync, sync->members[0].monitor, rect.left + 48, rect.top + 48, sync->sample->options->width,
                          sync->sample->options->height);
        }
        return true;
    case 'W':
        CloseSyncMember(sync, sync->memberCount - 1, "closed");
        return true;
    case 'K':
        if (sync->memberCount > 0)
        {
            DestroyWindow(sync->members[sync->memberCount - 1].hwnd);
            printf("KILL member=%d window destroyed, view still grouped\n", sync->memberCount - 1);
            fflush(stdout);
            SetTimer(sync->controller, kReapMemberTimerId, 1000, NULL);
        }
        return true;
    case VK_ESCAPE:
        PostQuitMessage(0);
        return true;
    default:
        return false;
    }
}

static LRESULT CALLBACK SyncWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SyncState* sync = &g_sync;

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        ValidateRect(hwnd, NULL);
        return 0;

    case WM_DPICHANGED:
    {
        const RECT* suggested = (const RECT*)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DISPLAYCHANGE:
        if (sync->group != NULL)
        {
            printf("DISPLAYCHANGE\n");
            fflush(stdout);
            PostMessageW(sync->controller, kReconcileMessage, 0, 0);
        }
        return 0;

    case WM_KEYDOWN:
        if (sync->group != NULL && HandleSyncKey(sync, wParam))
            return 0;
        break;

    case WM_TIMER:
        if (sync->group == NULL)
            break;

        if (wParam == kLevelsTimerId)
        {
            sync->levelTicks = (sync->levelTicks + 1) % 4;
            PollLevels(sync, sync->sample->options->levelsLog && sync->levelTicks == 0);
            UpdateSyncTitles(sync);
            return 0;
        }

        if (wParam != kCycleTimerId)
            KillTimer(hwnd, wParam);

        if (wParam == kCycleTimerId)
            StepSyncPreset(sync, 1);
        else if (wParam == kPauseTimerId)
            SetSyncPaused(sync, true);
        else if (wParam == kSnapshotTimerId)
            SnapshotMembers(sync);
        else if (wParam == kGroupToggleTimerId)
            ToggleGrouped(sync);
        else if (wParam == kCloseMemberTimerId)
            CloseSyncMember(sync, sync->memberCount - 1, "closed");
        else if (wParam == kAddMemberTimerId)
            HandleSyncKey(sync, 'A');
        else if (wParam == kSwitchSourceTimerId)
            HandleSyncKey(sync, 'M');
        else if (wParam == kKillMemberTimerId)
            HandleSyncKey(sync, 'K');
        else if (wParam == kReapMemberTimerId && sync->memberCount > 0)
        {
            svw_view* view = sync->members[sync->memberCount - 1].view;
            const sv_status status = svw_view_resume(view);
            printf("REAP member=%d status=%d last_error=%s\n", sync->memberCount - 1, (int)status, svw_last_error(view));
            fflush(stdout);
            CloseSyncMember(sync, sync->memberCount - 1, "window-destroyed");
        }
        else if (wParam == kExitTimerId)
            PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
    {
        const int index = FindSyncMember(sync, hwnd);
        if (index >= 0)
            CloseSyncMember(sync, index, "closed");
        else
            DestroyWindow(hwnd);
        return 0;
    }

    default:
        if (message == kReconcileMessage && hwnd == sync->controller)
        {
            ReconcileMembers(sync);
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static int RunSync(SampleState* state)
{
    SyncState* sync = &g_sync;
    const SampleOptions* options = state->options;
    sync->sample = state;
    sync->grouped = true;
    sync->scaling = options->scaling;
    sync->gain = options->gain;
    state->maxFps = options->maxFps;

    WNDCLASSEXW windowClass;
    memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = SyncWindowProc;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    windowClass.lpszClassName = kSyncWindowClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        fprintf(stderr, "RegisterClassExW failed (%lu)\n", GetLastError());
        return 1;
    }

    sync->controller = CreateWindowExW(0, kSyncWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    if (sync->controller == NULL)
    {
        fprintf(stderr, "CreateWindowExW (controller) failed (%lu)\n", GetLastError());
        return 1;
    }

    ToUtf8(options->resourcesDir, sync->resources, kUtf8PathBytes);
    sync->engine = EngineConfig(options, sync->resources);

    svw_group_config groupConfig;
    memset(&groupConfig, 0, sizeof(groupConfig));
    groupConfig.adapter = options->adapter;
    if (svw_group_create(&groupConfig, &sync->engine, &sync->group) != SV_OK)
    {
        fprintf(stderr, "svw_group_create failed: %s\n", svw_last_error(NULL));
        return 1;
    }

    if (options->fpsLog && svw_group_invoke(sync->group, EnableDebugLog, NULL) != SV_OK)
        fprintf(stderr, "svw_group_invoke failed: %s\n", svw_group_last_error(sync->group));

    LoadSyncPreset(sync, 0.0f);
    ReconcileMembers(sync);

    if (options->audio)
        StartAudio(sync, options->microphone ? SVW_AUDIO_MICROPHONE : SVW_AUDIO_LOOPBACK);
    UpdateSyncTitles(sync);

    const HWND timers = sync->controller;
    SetTimer(timers, kLevelsTimerId, 250, NULL);
    ArmTimer(timers, kCycleTimerId, options->cycleSeconds);
    ArmTimer(timers, kPauseTimerId, options->pauseAfterSeconds);
    ArmTimer(timers, kSnapshotTimerId, options->snapshotAfterSeconds);
    ArmTimer(timers, kExitTimerId, options->exitAfterSeconds);
    ArmTimer(timers, kGroupToggleTimerId, options->ungroupAfterSeconds);
    ArmTimer(timers, kCloseMemberTimerId, options->closeMemberAfterSeconds);
    ArmTimer(timers, kAddMemberTimerId, options->addMemberAfterSeconds);
    ArmTimer(timers, kSwitchSourceTimerId, options->switchSourceAfterSeconds);
    ArmTimer(timers, kKillMemberTimerId, options->killMemberAfterSeconds);

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (sync->input != NULL)
        svw_audio_input_destroy(sync->input);

    sync->reconciling = true;
    while (sync->memberCount > 0)
        CloseSyncMember(sync, sync->memberCount - 1, "exit");
    svw_group_destroy(sync->group);
    sync->group = NULL;
    DestroyWindow(sync->controller);
    printf("SYNC DONE\n");
    fflush(stdout);
    return 0;
}

static void PrintUsage(void)
{
    printf("synesthesia-vibe-windows-sample [options]\n"
           "  --presets DIR           folder of .milk presets (default: resources\\mvp-presets above the exe)\n"
           "  --resources DIR         engine resource path with textures\\ (default: resources\\beatdrop)\n"
           "  --preset TEXT           only presets whose file name contains TEXT\n"
           "  --size WxH              client size in physical pixels\n"
           "  --scale S               render scale, 0.25 to 1\n"
           "  --max-fps N             frame-rate cap (0 = monitor refresh)\n"
           "  --max-luminance L       luminance limit at creation\n"
           "  --flash-limit F         flash limit at creation\n"
           "  --warp                  ANGLE WARP (software Direct3D 11) adapter\n"
           "  --cycle-seconds S       next preset every S seconds (0 = off, default 15)\n"
           "  --fps-log               print the engine's display fps every second\n"
           "  --pause-after S         pause after S seconds\n"
           "  --snapshot-after S      write a snapshot PNG after S seconds\n"
           "  --snapshot-dir DIR      folder for snapshot PNGs (default: current folder)\n"
           "  --exit-after S          close after S seconds\n"
           "  --capture DIR           render every preset deterministically and write DIR\\<preset>.png, then exit\n"
           "  --frames N              frames rendered per capture (default 120)\n"
           "  --sync                  one window per monitor (two on one monitor) in a sync group, loopback audio on\n"
           "  --second-size WxH       --sync: client size of the second window on a single monitor\n"
           "  --scaling MODE          --sync: fill (default), fit or stretch\n"
           "  --gain G                --sync: audio input gain (default 1)\n"
           "  --mic                   --sync: start with the microphone instead of loopback\n"
           "  --no-audio              --sync: no audio input\n"
           "  --levels-log            --sync: print the audio levels every second\n"
           "  --ungroup-after S       --sync: leave (or rejoin) the group after S seconds\n"
           "  --close-member-after S  --sync: close the last member window after S seconds\n"
           "  --add-member-after S    --sync: open one more member window after S seconds\n"
           "  --switch-source-after S --sync: switch loopback/microphone after S seconds\n"
           "  --kill-member-after S   --sync: destroy the last member's window without leaving the group\n"
           "Keys: Space pause/resume, Right/N next, Left/P previous, 1-4 scale 0.25/0.5/0.75/1,\n"
           "      C cap 30/60/refresh, S snapshot, Esc quit\n"
           "Sync keys: F fill/fit/stretch, G leave/join group, Up/Down gain, M microphone/loopback,\n"
           "      A add a window, W close a window, K destroy a window without leaving\n");
}

static bool ParseOptions(int argc, wchar_t** argv, SampleOptions* options)
{
    memset(options, 0, sizeof(*options));
    options->width = kDefaultWidth;
    options->height = kDefaultHeight;
    options->frames = kDefaultCaptureFrames;
    options->renderScale = 1.0f;
    options->cycleSeconds = 15.0;
    options->gain = 1.0f;
    options->audio = true;
    bool sizeGiven = false;

    for (int index = 1; index < argc; ++index)
    {
        const wchar_t* flag = argv[index];
        const wchar_t* value = index + 1 < argc ? argv[index + 1] : NULL;

        if (wcscmp(flag, L"--warp") == 0)
        {
            options->adapter = SVW_ADAPTER_WARP;
            continue;
        }
        if (wcscmp(flag, L"--fps-log") == 0)
        {
            options->fpsLog = true;
            continue;
        }
        if (wcscmp(flag, L"--sync") == 0)
        {
            options->sync = true;
            continue;
        }
        if (wcscmp(flag, L"--mic") == 0)
        {
            options->microphone = true;
            continue;
        }
        if (wcscmp(flag, L"--no-audio") == 0)
        {
            options->audio = false;
            continue;
        }
        if (wcscmp(flag, L"--levels-log") == 0)
        {
            options->levelsLog = true;
            continue;
        }
        if (wcscmp(flag, L"--help") == 0 || value == NULL)
            return false;

        ++index;
        if (wcscmp(flag, L"--presets") == 0)
            wcsncpy_s(options->presetsDir, kPathChars, value, _TRUNCATE);
        else if (wcscmp(flag, L"--resources") == 0)
            wcsncpy_s(options->resourcesDir, kPathChars, value, _TRUNCATE);
        else if (wcscmp(flag, L"--preset") == 0)
            wcsncpy_s(options->presetFilter, kPathChars, value, _TRUNCATE);
        else if (wcscmp(flag, L"--capture") == 0)
            wcsncpy_s(options->captureDir, kPathChars, value, _TRUNCATE);
        else if (wcscmp(flag, L"--snapshot-dir") == 0)
            wcsncpy_s(options->snapshotDir, kPathChars, value, _TRUNCATE);
        else if (wcscmp(flag, L"--size") == 0)
            sizeGiven = swscanf_s(value, L"%dx%d", &options->width, &options->height) == 2;
        else if (wcscmp(flag, L"--scale") == 0)
            options->renderScale = (float)_wtof(value);
        else if (wcscmp(flag, L"--max-fps") == 0)
            options->maxFps = (float)_wtof(value);
        else if (wcscmp(flag, L"--max-luminance") == 0)
            options->maxLuminance = (float)_wtof(value);
        else if (wcscmp(flag, L"--flash-limit") == 0)
            options->flashLimit = (float)_wtof(value);
        else if (wcscmp(flag, L"--cycle-seconds") == 0)
            options->cycleSeconds = _wtof(value);
        else if (wcscmp(flag, L"--pause-after") == 0)
            options->pauseAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--snapshot-after") == 0)
            options->snapshotAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--exit-after") == 0)
            options->exitAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--frames") == 0)
            options->frames = _wtoi(value);
        else if (wcscmp(flag, L"--second-size") == 0)
            swscanf_s(value, L"%dx%d", &options->secondWidth, &options->secondHeight);
        else if (wcscmp(flag, L"--scaling") == 0)
            options->scaling = wcscmp(value, L"fit") == 0 ? SVW_SYNC_SCALING_FIT
                             : wcscmp(value, L"stretch") == 0 ? SVW_SYNC_SCALING_STRETCH : SVW_SYNC_SCALING_FILL;
        else if (wcscmp(flag, L"--gain") == 0)
            options->gain = (float)_wtof(value);
        else if (wcscmp(flag, L"--ungroup-after") == 0)
            options->ungroupAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--close-member-after") == 0)
            options->closeMemberAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--add-member-after") == 0)
            options->addMemberAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--switch-source-after") == 0)
            options->switchSourceAfterSeconds = _wtof(value);
        else if (wcscmp(flag, L"--kill-member-after") == 0)
            options->killMemberAfterSeconds = _wtof(value);
        else
            return false;
    }

    if (options->captureDir[0] != L'\0' && !sizeGiven)
    {
        options->width = kCaptureWidth;
        options->height = kCaptureHeight;
    }
    else if (options->sync && !sizeGiven)
    {
        options->width = kSyncDefaultWidth;
        options->height = kSyncDefaultHeight;
    }

    return options->width > 0 && options->height > 0 && options->frames > 0;
}

int wmain(int argc, wchar_t** argv)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    static SampleOptions options;
    if (!ParseOptions(argc, argv, &options))
    {
        PrintUsage();
        return 2;
    }

    FindDefaultResources(&options);
    if (options.presetsDir[0] == L'\0' || !DirectoryExists(options.presetsDir))
    {
        fprintf(stderr, "no preset folder: pass --presets DIR\n");
        return 2;
    }

    WNDCLASSEXW windowClass;
    memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        fprintf(stderr, "RegisterClassExW failed (%lu)\n", GetLastError());
        return 1;
    }

    g_state.options = &options;
    if (ListPresets(&g_state) == 0)
    {
        char presetsUtf8[kUtf8PathBytes];
        ToUtf8(options.presetsDir, presetsUtf8, kUtf8PathBytes);
        fprintf(stderr, "no .milk presets in %s\n", presetsUtf8);
        return 2;
    }

    if (options.captureDir[0] != L'\0')
        return RunCapture(&g_state);

    return options.sync ? RunSync(&g_state) : RunInteractive(&g_state);
}
