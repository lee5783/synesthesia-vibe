//
//  render_view.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#include "render_view.h"

#include "windows_headers.h"
#include "gl_headers.h"

#include "audio_router.h"
#include "host_support.h"
#include "snapshot_writer.h"
#include "sync_group.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <utility>

namespace
{
    using svw::ClampRenderScale;
    using svw::ErrorText;
    using svw::ScopedEvent;
    using svw::WaitServicingSentMessages;
    using svw::kCoreStackHeadroomBytes;
    using svw::kMaxRenderScale;
    using svw::kRendererUnavailablePrefix;

    thread_local svw_view* t_renderView = nullptr;

    void CALLBACK OnLocationChange(HWINEVENTHOOK, DWORD, HWND hwnd, LONG objectId, LONG childId, DWORD, DWORD)
    {
        if (objectId != OBJID_WINDOW || childId != CHILDID_SELF || t_renderView == nullptr)
            return;

        t_renderView->NoteWindowMoved(hwnd);
    }

    class GroupRef
    {
    public:
        explicit GroupRef(svw_group* group) : m_group(group) {}
        ~GroupRef()
        {
            if (m_group != nullptr)
                m_group->Release();
        }

        GroupRef(const GroupRef&) = delete;
        GroupRef& operator=(const GroupRef&) = delete;

        svw_group* Get() const { return m_group; }

    private:
        svw_group* m_group;
    };
}

sv_status svw_view::Create(const svw_view_config& config, const sv_config* engine, svw_view*& outView, std::string& error)
{
    outView = nullptr;

    const HWND hwnd = static_cast<HWND>(config.hwnd);
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        error = "hwnd is not a window";
        return SV_ERR_ARG;
    }

    if (config.adapter != SVW_ADAPTER_DEFAULT && config.adapter != SVW_ADAPTER_WARP)
    {
        error = "adapter must be SVW_ADAPTER_DEFAULT or SVW_ADAPTER_WARP";
        return SV_ERR_ARG;
    }

    if (!std::isfinite(config.render_scale))
    {
        error = "render_scale is not finite";
        return SV_ERR_ARG;
    }

    std::unique_ptr<svw_view> view(new svw_view(config, engine));
    view->m_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    view->m_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (view->m_wake == nullptr || view->m_ready == nullptr || !view->m_pacer.Valid())
    {
        error = "could not create the render thread's events or timer";
        return SV_ERR_GL;
    }

    view->m_thread = std::thread(&svw_view::ThreadMain, view.get());
    WaitServicingSentMessages(view->m_ready);

    if (view->m_createStatus != SV_OK)
    {
        error = view->m_createError;
        return view->m_createStatus;
    }

    outView = view.release();
    return SV_OK;
}

svw_view::svw_view(const svw_view_config& config, const sv_config* engine)
    : m_hwnd(static_cast<HWND>(config.hwnd))
    , m_adapter(config.adapter)
    , m_engine()
    , m_threadId(0)
    , m_wake(nullptr)
    , m_ready(nullptr)
    , m_createStatus(SV_ERR_GL)
    , m_threadExited(false)
    , m_stopping(false)
    , m_pauseRequested(config.start_paused)
    , m_renderScale(config.render_scale > 0.0f ? ClampRenderScale(config.render_scale) : kMaxRenderScale)
    , m_preferredFps(0.0f)
    , m_playbackRate(1.0f)
    , m_surface(EGL_NO_SURFACE)
    , m_context(EGL_NO_CONTEXT)
    , m_ctx(nullptr)
    , m_winEventHook(nullptr)
    , m_glLock(nullptr)
    , m_surfaceWidth(0)
    , m_surfaceHeight(0)
    , m_backingWidth(0)
    , m_backingHeight(0)
    , m_ctxPaused(false)
    , m_presentPending(false)
    , m_failed(false)
    , m_failStatus(SV_OK)
    , m_lastPresetFromMemory(false)
    , m_group(nullptr)
    , m_parkedFor(nullptr)
    , m_syncScaling(SVW_SYNC_SCALING_FILL)
{
    if (engine != nullptr)
    {
        m_engine = *engine;
        m_resourcePath = engine->resource_path != nullptr ? engine->resource_path : "";
        m_textureSearchPath = engine->texture_search_path != nullptr ? engine->texture_search_path : "";
        m_cachePath = engine->cache_path != nullptr ? engine->cache_path : "";
    }

    m_preferredFps = std::isfinite(m_engine.max_fps) && m_engine.max_fps > 0.0f ? m_engine.max_fps : 0.0f;
}

svw_view::~svw_view()
{
    if (m_thread.joinable())
    {
        m_stopping = true;
        if (m_wake != nullptr)
            SetEvent(m_wake);

        WaitServicingSentMessages(static_cast<HANDLE>(m_thread.native_handle()));
        m_thread.join();
    }

    if (m_wake != nullptr)
        CloseHandle(m_wake);
    if (m_ready != nullptr)
        CloseHandle(m_ready);
}

sv_status svw_view::LoadPreset(const char* path, float blendSeconds)
{
    if (path == nullptr || path[0] == '\0')
        return Fail(SV_ERR_ARG, "svw_view_load_preset: path is empty");

    const GroupRef group(AcquireGroup());
    if (group.Get() != nullptr)
    {
        const sv_status status = group.Get()->LoadPreset(path, blendSeconds);
        return status == SV_OK ? SV_OK : Fail(status, group.Get()->LastError());
    }

    const std::string presetPath(path);
    return RunSync([this, presetPath, blendSeconds]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        const sv_status status = sv_load_preset(m_ctx, presetPath.c_str(), blendSeconds);
        if (status != SV_OK)
            return Fail(status, ErrorText(m_ctx, "sv_load_preset failed"));

        m_lastPresetPath = presetPath;
        m_lastPresetText.clear();
        m_lastPresetFromMemory = false;
        return SV_OK;
    });
}

sv_status svw_view::LoadPresetMemory(const char* text, size_t len, float blendSeconds)
{
    if (text == nullptr || len == 0)
        return Fail(SV_ERR_ARG, "svw_view_load_preset_memory: text is empty");

    const GroupRef group(AcquireGroup());
    if (group.Get() != nullptr)
    {
        const sv_status status = group.Get()->LoadPresetMemory(text, len, blendSeconds);
        return status == SV_OK ? SV_OK : Fail(status, group.Get()->LastError());
    }

    const std::string presetText(text, len);
    return RunSync([this, presetText, blendSeconds]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        const sv_status status = sv_load_preset_memory(m_ctx, presetText.data(), presetText.size(), blendSeconds);
        if (status != SV_OK)
            return Fail(status, ErrorText(m_ctx, "sv_load_preset_memory failed"));

        m_lastPresetText = presetText;
        m_lastPresetPath.clear();
        m_lastPresetFromMemory = true;
        return SV_OK;
    });
}

sv_status svw_view::Pause()
{
    m_pauseRequested = true;
    SetEvent(m_wake);
    WakeGroup();
    return SV_OK;
}

sv_status svw_view::Resume()
{
    m_pauseRequested = false;
    SetEvent(m_wake);
    WakeGroup();
    return SV_OK;
}

sv_status svw_view::SetPlaybackRate(float rate)
{
    if (!std::isfinite(rate))
        return Fail(SV_ERR_ARG, "svw_view_set_playback_rate: rate is not finite");

    m_playbackRate = rate;
    Post([this, rate]() {
        if (m_ctx != nullptr)
            sv_set_playback_rate(m_ctx, rate);
    });
    return SV_OK;
}

sv_status svw_view::SetPreferredFps(float fps)
{
    if (std::isnan(fps))
        return Fail(SV_ERR_ARG, "svw_view_set_preferred_fps: fps is NaN");

    m_preferredFps = std::isfinite(fps) && fps > 0.0f ? fps : 0.0f;
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_view::SetRenderScale(float scale)
{
    if (!std::isfinite(scale))
        return Fail(SV_ERR_ARG, "svw_view_set_render_scale: scale is not finite");

    m_renderScale = ClampRenderScale(scale);
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_view::Snapshot(svw_snapshot_fn fn, void* user)
{
    if (fn == nullptr)
        return Fail(SV_ERR_ARG, "svw_view_snapshot: fn is null");

    return Enqueue(SnapshotRequest{ fn, user, std::string(), nullptr });
}

sv_status svw_view::SnapshotPng(const char* path)
{
    if (path == nullptr || path[0] == '\0')
        return Fail(SV_ERR_ARG, "svw_view_snapshot_png: path is empty");

    return Enqueue(SnapshotRequest{ nullptr, nullptr, std::string(path), nullptr });
}

sv_status svw_view::Invoke(svw_invoke_fn fn, void* user)
{
    if (fn == nullptr)
        return Fail(SV_ERR_ARG, "svw_view_invoke: fn is null");

    const GroupRef group(AcquireGroup());
    if (group.Get() != nullptr)
    {
        const sv_status status = group.Get()->Invoke(fn, user);
        return status == SV_OK ? SV_OK : Fail(status, group.Get()->LastError());
    }

    return RunSync([this, fn, user]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        fn(m_ctx, CurrentTarget(), user);
        return SV_OK;
    });
}

bool svw_view::OnRenderThread() const
{
    return GetCurrentThreadId() == m_threadId.load();
}

std::string svw_view::LastError() const
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

sv_status svw_view::Fail(sv_status status, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = message;
    return status;
}

void svw_view::NoteWindowMoved(HWND hwnd)
{
    if (hwnd != m_hwnd)
        return;

    m_presentPending = true;
    WakeGroup();
}

sv_status svw_view::RunSync(std::function<sv_status()> task)
{
    if (OnRenderThread())
        return task();

    ScopedEvent done;
    if (done.Get() == nullptr)
        return Fail(SV_ERR_GL, "could not create a completion event");

    Completion completion{ done.Get(), SV_ERR_GL };
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_threadExited)
            return Fail(SV_ERR_GL, "the render thread has stopped");

        m_commands.push_back(Command{ std::move(task), &completion });
    }

    SetEvent(m_wake);
    WaitServicingSentMessages(done.Get());
    return completion.status;
}

void svw_view::Post(std::function<void()> task)
{
    if (OnRenderThread())
    {
        task();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_threadExited)
            return;

        m_commands.push_back(Command{ [task]() { task(); return SV_OK; }, nullptr });
    }

    SetEvent(m_wake);
}

sv_status svw_view::Enqueue(SnapshotRequest request)
{
    const GroupRef group(AcquireGroup());
    if (group.Get() != nullptr && group.Get()->OnRenderThread() && !request.pngPath.empty())
        return Fail(SV_ERR_ARG, "svw_view_snapshot_png: a grouped member cannot be written from the group's render thread");

    if (group.Get() == nullptr && OnRenderThread())
    {
        if (m_ctx == nullptr)
            return CurrentFailure();

        ScopedEvent done;
        Completion completion{ done.Get(), SV_OK };
        request.completion = &completion;
        std::vector<SnapshotRequest> requests;
        requests.push_back(std::move(request));
        ServeSnapshots(requests, CurrentTarget(), m_backingWidth, m_backingHeight, false);
        return completion.status;
    }

    const char* unpresentable = IsWindow(m_hwnd) ? svw::UnpresentableReason(m_hwnd) : nullptr;
    if (unpresentable != nullptr)
        return Fail(SV_ERR_GL, std::string(request.pngPath.empty() ? "svw_view_snapshot: " : "svw_view_snapshot_png: ") + unpresentable);

    const bool waits = !request.pngPath.empty();
    ScopedEvent done;
    Completion completion{ done.Get(), SV_ERR_GL };
    if (waits)
    {
        if (done.Get() == nullptr)
            return Fail(SV_ERR_GL, "could not create a completion event");

        request.completion = &completion;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_threadExited)
            return Fail(SV_ERR_GL, "the render thread has stopped");

        m_snapshots.push_back(std::move(request));
    }

    SetEvent(m_wake);
    if (group.Get() != nullptr)
        group.Get()->Wake();
    if (!waits)
        return SV_OK;

    WaitServicingSentMessages(done.Get());
    return completion.status;
}

sv_status svw_view::CurrentFailure()
{
    if (m_failed)
        return Fail(m_failStatus, m_failMessage);

    return Fail(SV_ERR_GL, "the view has no GL context");
}

void svw_view::FailFatal(sv_status status, const std::string& message)
{
    m_failed = true;
    m_failStatus = status;
    m_failMessage = message;
    Fail(status, message);
    TearDownGraphics();
}

void svw_view::ThreadMain()
{
    volatile char* headroom = static_cast<volatile char*>(_alloca(kCoreStackHeadroomBytes));
    headroom[0] = 0;
    headroom[kCoreStackHeadroomBytes - 1] = 0;
    RunThread();
}

void svw_view::RunThread()
{
    m_threadId = GetCurrentThreadId();
    t_renderView = this;
    svw::MarkHostRenderThread(true);
    SetThreadDescription(GetCurrentThread(), L"synesthesia-vibe render");

    const DPI_AWARENESS_CONTEXT awareness = GetWindowDpiAwarenessContext(m_hwnd);
    if (awareness != nullptr)
        SetThreadDpiAwarenessContext(awareness);

    std::unique_lock<std::mutex> glLock(svw::HostGlMutex());
    m_glLock = &glLock;

    std::string error;
    if (!svw::AcquireDisplay(m_adapter, m_lease, error) || !BuildGraphics(error))
    {
        m_glLock = nullptr;
        TearDownGraphics();
        svw::ReleaseDisplay(m_lease);
        eglReleaseThread();
        m_createError = error;
        m_createStatus = IsWindow(m_hwnd) ? SV_ERR_GL : SV_ERR_ARG;
        FailPendingWork();
        t_renderView = nullptr;
        svw::MarkHostRenderThread(false);
        SetEvent(m_ready);
        return;
    }

    DWORD processId = 0;
    const DWORD windowThreadId = GetWindowThreadProcessId(m_hwnd, &processId);
    m_winEventHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, OnLocationChange,
                                     processId, windowThreadId, WINEVENT_OUTOFCONTEXT);

    m_createStatus = SV_OK;
    SetEvent(m_ready);

    RunLoop();

    if (m_winEventHook != nullptr)
        UnhookWinEvent(m_winEventHook);

    TearDownGraphics();
    svw::ReleaseDisplay(m_lease);
    eglReleaseThread();
    FailPendingWork();
    m_glLock = nullptr;
    t_renderView = nullptr;
    svw::MarkHostRenderThread(false);
}

void svw_view::RunLoop()
{
    double deadline = 0.0;
    double lastFrameAt = 0.0;

    for (;;)
    {
        if (m_ctx != nullptr && m_parkedFor == nullptr && svw::DisplayStale(m_lease))
            Rebuild();

        PumpMessages();
        DrainCommands();
        if (m_stopping)
            return;

        if (m_ctx == nullptr || m_parkedFor != nullptr)
        {
            if (m_ctx == nullptr)
                FailPendingSnapshots();
            WaitForWork(false);
            continue;
        }

        if (!IsWindow(m_hwnd))
        {
            FailFatal(SV_ERR_ARG, "the window was destroyed");
            continue;
        }

        const double now = m_pacer.Now();
        m_pacer.SetPreferredFps(m_preferredFps.load());
        if (m_pacer.RefreshDue(m_hwnd, now))
        {
            const bool ownsCurrent = ReleaseGlTurn();
            const bool refreshChanged = m_pacer.TrackMonitor(m_hwnd, now);
            TakeGlTurn(ownsCurrent);
            if (refreshChanged)
                deadline = now;
            continue;
        }

        if (ApplyPauseState())
        {
            lastFrameAt = 0.0;
            deadline = now;
        }

        if (IsIconic(m_hwnd) || !UpdateGeometry())
        {
            const char* reason = svw::UnpresentableReason(m_hwnd);
            RejectPendingSnapshots(reason != nullptr ? reason : "the window has no client area");
            svw::WithdrawAudioLevels(m_ctx);
            WaitForWork(false);
            continue;
        }

        if (m_ctxPaused)
        {
            if (m_presentPending || HasPendingSnapshots())
            {
                m_presentPending = false;
                RenderAndPresent(0.0);
            }
            else
            {
                WaitForWork(false);
            }
            continue;
        }

        if (now < deadline)
        {
            m_pacer.Arm(deadline - now);
            WaitForWork(true);
            continue;
        }

        const double period = m_pacer.PeriodSeconds();
        deadline = deadline + period > now ? deadline + period : now + period;
        const double dtSeconds = lastFrameAt > 0.0 ? now - lastFrameAt : 0.0;
        lastFrameAt = now;
        RenderAndPresent(dtSeconds);
    }
}

void svw_view::PumpMessages()
{
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        DispatchMessageW(&message);
}

void svw_view::DrainCommands()
{
    std::vector<Command> commands;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        commands.swap(m_commands);
    }

    for (Command& command : commands)
    {
        const sv_status status = command.run();
        if (command.completion != nullptr)
        {
            command.completion->status = status;
            SetEvent(command.completion->event);
        }
    }
}

bool svw_view::ReleaseGlTurn()
{
    const bool ownsCurrent = m_context != EGL_NO_CONTEXT && m_parkedFor == nullptr;
    if (ownsCurrent)
        eglMakeCurrent(m_lease.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_glLock != nullptr)
        m_glLock->unlock();
    return ownsCurrent;
}

void svw_view::TakeGlTurn(bool ownsCurrent)
{
    if (m_glLock != nullptr)
        m_glLock->lock();
    if (ownsCurrent && m_context != EGL_NO_CONTEXT && m_parkedFor == nullptr && !svw::DisplayStale(m_lease))
        eglMakeCurrent(m_lease.display, m_surface, m_surface, m_context);
}

void svw_view::WaitForWork(bool withTimer)
{
    const HANDLE handles[2] = { m_wake, m_pacer.Timer() };
    const bool ownsCurrent = ReleaseGlTurn();
    MsgWaitForMultipleObjectsEx(withTimer ? 2 : 1, handles, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    TakeGlTurn(ownsCurrent);
}

void svw_view::WritePendingPngs()
{
    if (!HasPngWrites())
        return;

    const bool ownsCurrent = ReleaseGlTurn();
    FlushPngWrites();
    TakeGlTurn(ownsCurrent);
}

void svw_view::FailPendingWork()
{
    std::vector<Command> commands;
    std::vector<SnapshotRequest> snapshots;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_threadExited = true;
        commands.swap(m_commands);
        snapshots.swap(m_snapshots);
    }

    Fail(SV_ERR_GL, m_failed ? m_failMessage : std::string("the render thread has stopped"));

    for (Command& command : commands)
    {
        if (command.completion != nullptr)
        {
            command.completion->status = SV_ERR_GL;
            SetEvent(command.completion->event);
        }
    }

    for (SnapshotRequest& request : snapshots)
    {
        if (request.completion != nullptr)
        {
            request.completion->status = SV_ERR_GL;
            SetEvent(request.completion->event);
        }
    }

    FlushPngWrites();
}

bool svw_view::BuildGraphics(std::string& error)
{
    const EGLDisplay display = m_lease.display;

    m_surface = eglCreateWindowSurface(display, m_lease.config, reinterpret_cast<EGLNativeWindowType>(m_hwnd), nullptr);
    if (m_surface == EGL_NO_SURFACE)
    {
        error = svw::DescribeEglError("eglCreateWindowSurface", eglGetError());
        return false;
    }

    m_context = svw::CreateRenderContext(m_lease, error);
    if (m_context == EGL_NO_CONTEXT)
        return false;

    if (eglMakeCurrent(display, m_surface, m_surface, m_context) != EGL_TRUE)
    {
        error = svw::DescribeEglError("eglMakeCurrent", eglGetError());
        return false;
    }

    eglSwapInterval(display, 1);

    if (!svw::LoadGlFunctionsOnce(error))
        return false;

    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    eglQuerySurface(display, m_surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(display, m_surface, EGL_HEIGHT, &surfaceHeight);
    surfaceWidth = std::max<EGLint>(1, surfaceWidth);
    surfaceHeight = std::max<EGLint>(1, surfaceHeight);

    const float scale = m_renderScale.load();
    const int backingWidth = std::max(1, static_cast<int>(std::lround(surfaceWidth * scale)));
    const int backingHeight = std::max(1, static_cast<int>(std::lround(surfaceHeight * scale)));

    m_pacer.SetPreferredFps(m_preferredFps.load());
    m_pacer.TrackMonitor(m_hwnd, m_pacer.Now());

    sv_config config = m_engine;
    config.width = backingWidth;
    config.height = backingHeight;
    config.resource_path = m_resourcePath.empty() ? nullptr : m_resourcePath.c_str();
    config.texture_search_path = m_textureSearchPath.empty() ? nullptr : m_textureSearchPath.c_str();
    config.cache_path = m_cachePath.empty() ? nullptr : m_cachePath.c_str();
    if (!(config.fps_hint > 0.0f))
        config.fps_hint = static_cast<float>(m_pacer.EffectiveFps());

    m_ctx = sv_create(&config);
    if (m_ctx == nullptr)
    {
        error = "sv_create failed";
        return false;
    }

    const std::string createError = ErrorText(m_ctx, "");
    if (createError.compare(0, strlen(kRendererUnavailablePrefix), kRendererUnavailablePrefix) == 0)
    {
        error = createError;
        return false;
    }

    m_surfaceWidth = surfaceWidth;
    m_surfaceHeight = surfaceHeight;
    m_backingWidth = backingWidth;
    m_backingHeight = backingHeight;

    if ((backingWidth != surfaceWidth || backingHeight != surfaceHeight) && !m_scaled.Create(backingWidth, backingHeight))
    {
        error = "could not create the render-scale framebuffer";
        return false;
    }

    svw::RegisterAudioContext(m_ctx);
    sv_set_playback_rate(m_ctx, m_playbackRate.load());

    m_ctxPaused = m_pauseRequested.load();
    if (m_ctxPaused)
        sv_pause(m_ctx);

    m_presentPending = false;
    return true;
}

void svw_view::TearDownGraphics()
{
    const EGLDisplay display = m_lease.display;
    const bool handlesLive = display != EGL_NO_DISPLAY && !svw::DisplayStale(m_lease);

    if (handlesLive && m_context != EGL_NO_CONTEXT)
        eglMakeCurrent(display, m_surface, m_surface, m_context);

    m_scaled.Destroy();

    if (m_ctx != nullptr)
    {
        svw::UnregisterAudioContext(m_ctx);
        sv_destroy(m_ctx);
        m_ctx = nullptr;
    }

    if (display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (handlesLive && m_surface != EGL_NO_SURFACE)
            eglDestroySurface(display, m_surface);
        if (handlesLive && m_context != EGL_NO_CONTEXT)
            eglDestroyContext(display, m_context);
    }

    m_surface = EGL_NO_SURFACE;
    m_context = EGL_NO_CONTEXT;
    m_surfaceWidth = 0;
    m_surfaceHeight = 0;
    m_backingWidth = 0;
    m_backingHeight = 0;
}

void svw_view::Rebuild()
{
    TearDownGraphics();

    std::string error;
    if (!svw::RecoverDisplay(m_lease, error) || !BuildGraphics(error))
    {
        FailFatal(SV_ERR_GL, "the GL context was lost and could not be rebuilt: " + error);
        return;
    }

    ReloadLastPreset();
}

void svw_view::ReloadLastPreset()
{
    sv_status status = SV_OK;
    if (m_lastPresetFromMemory && !m_lastPresetText.empty())
        status = sv_load_preset_memory(m_ctx, m_lastPresetText.data(), m_lastPresetText.size(), 0.0f);
    else if (!m_lastPresetPath.empty())
        status = sv_load_preset(m_ctx, m_lastPresetPath.c_str(), 0.0f);

    if (status != SV_OK)
        Fail(status, "the GL context was rebuilt but the last preset did not reload: " + ErrorText(m_ctx, "unknown error"));
    else
        Fail(SV_OK, "the GL context was lost and has been rebuilt");
}

bool svw_view::ApplyPauseState()
{
    const bool wantPaused = m_pauseRequested.load();
    if (wantPaused == m_ctxPaused)
        return false;

    m_ctxPaused = wantPaused;
    if (wantPaused)
    {
        sv_pause(m_ctx);
        svw::WithdrawAudioLevels(m_ctx);
        m_presentPending = false;
        return false;
    }

    sv_resume(m_ctx);
    return true;
}

bool svw_view::UpdateGeometry()
{
    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    eglQuerySurface(m_lease.display, m_surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(m_lease.display, m_surface, EGL_HEIGHT, &surfaceHeight);
    if (surfaceWidth <= 0 || surfaceHeight <= 0)
        return false;

    const float scale = m_renderScale.load();
    int backingWidth = std::max(1, static_cast<int>(std::lround(surfaceWidth * scale)));
    int backingHeight = std::max(1, static_cast<int>(std::lround(surfaceHeight * scale)));

    m_surfaceWidth = surfaceWidth;
    m_surfaceHeight = surfaceHeight;

    if (backingWidth != surfaceWidth || backingHeight != surfaceHeight)
    {
        if (!m_scaled.Resize(backingWidth, backingHeight))
        {
            m_scaled.Destroy();
            Fail(SV_ERR_GL, "could not create the render-scale framebuffer; rendering at full size");
            backingWidth = surfaceWidth;
            backingHeight = surfaceHeight;
        }
    }
    else if (m_scaled.Valid())
    {
        m_scaled.Destroy();
    }

    if (backingWidth != m_backingWidth || backingHeight != m_backingHeight)
    {
        sv_resize(m_ctx, backingWidth, backingHeight);
        m_backingWidth = backingWidth;
        m_backingHeight = backingHeight;
        m_presentPending = true;
    }

    return true;
}

uint32_t svw_view::CurrentTarget() const
{
    return m_scaled.Valid() ? m_scaled.Id() : 0;
}

void svw_view::RenderAndPresent(double dtSeconds)
{
    const uint32_t target = CurrentTarget();
    sv_render_frame(m_ctx, dtSeconds, target);
    if (!m_ctxPaused)
        svw::PublishAudioLevels(m_ctx);

    if (target != 0)
        BlitToWindow();

    if (HasPendingSnapshots())
    {
        std::vector<SnapshotRequest> requests;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            requests.swap(m_snapshots);
        }
        ServeSnapshots(requests, target, m_backingWidth, m_backingHeight, true);
    }

    if (eglSwapBuffers(m_lease.display, m_surface) != EGL_TRUE)
    {
        const EGLint code = eglGetError();
        if (code == EGL_CONTEXT_LOST)
            Rebuild();
        else if (!IsWindow(m_hwnd))
            FailFatal(SV_ERR_ARG, "the window was destroyed");
        else
            FailFatal(SV_ERR_GL, svw::DescribeEglError("eglSwapBuffers", code));
    }

    WritePendingPngs();
}

void svw_view::BlitToWindow()
{
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    if (scissorEnabled)
        glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_scaled.Id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, m_backingWidth, m_backingHeight, 0, 0, m_surfaceWidth, m_surfaceHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (scissorEnabled)
        glEnable(GL_SCISSOR_TEST);
}

bool svw_view::HasPendingSnapshots()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return !m_snapshots.empty();
}

void svw_view::ServeSnapshots(std::vector<SnapshotRequest>& requests, uint32_t target, int width, int height, bool deferPngWrites)
{
    std::vector<uint8_t> pixels;
    const bool readOk = svw::ReadFramebufferTopDown(target, width, height, pixels);

    for (SnapshotRequest& request : requests)
    {
        sv_status status = SV_OK;
        if (!readOk)
        {
            status = Fail(SV_ERR_GL, "snapshot readback failed");
        }
        else if (request.fn != nullptr)
        {
            request.fn(pixels.data(), width, height, request.user);
        }
        else if (deferPngWrites)
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_pngWrites.push_back(PngWrite{ pixels, width, height, std::move(request.pngPath), request.completion });
            continue;
        }
        else
        {
            std::string error;
            if (!svw::WritePngUtf8(request.pngPath.c_str(), pixels, width, height, error))
                status = Fail(SV_ERR_IO, "svw_view_snapshot_png: " + error);
        }

        if (request.completion != nullptr)
        {
            request.completion->status = status;
            SetEvent(request.completion->event);
        }
    }
}

sv_status svw_view::SetSyncGroup(svw_group* group)
{
    if (svw::OnHostRenderThread())
        return Fail(SV_ERR_ARG, "svw_view_set_sync_group: called from a render thread");

    if (group != nullptr && group->Adapter() != m_adapter)
        return Fail(SV_ERR_ARG, "svw_view_set_sync_group: the group uses a different adapter");

    std::lock_guard<std::mutex> lock(svw::MembershipMutex());
    const GroupRef current(AcquireGroup());
    if (current.Get() == group)
        return SV_OK;

    if (current.Get() != nullptr)
        LeaveGroupLocked(current.Get());

    if (group == nullptr)
        return SV_OK;

    group->AddRef();
    {
        std::lock_guard<std::mutex> groupLock(m_groupMutex);
        m_group = group;
    }

    sv_status status = RunSync([this, group]() { return ParkFor(group); });
    if (status == SV_OK)
    {
        status = group->AddMember(this);
        if (status != SV_OK)
            Fail(status, "svw_view_set_sync_group: " + group->LastError());
    }

    if (status != SV_OK)
        LeaveGroupLocked(group);

    return status;
}

void svw_view::FailPendingSnapshots()
{
    std::vector<SnapshotRequest> requests;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        requests.swap(m_snapshots);
    }

    for (SnapshotRequest& request : requests)
    {
        const sv_status status = CurrentFailure();
        if (request.completion != nullptr)
        {
            request.completion->status = status;
            SetEvent(request.completion->event);
        }
    }
}

void svw_view::RejectPendingSnapshots(const char* reason)
{
    std::vector<SnapshotRequest> requests;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        requests.swap(m_snapshots);
    }

    for (SnapshotRequest& request : requests)
    {
        const sv_status status = Fail(SV_ERR_GL, std::string(request.pngPath.empty() ? "svw_view_snapshot: " : "svw_view_snapshot_png: ") + reason);
        if (request.completion != nullptr)
        {
            request.completion->status = status;
            SetEvent(request.completion->event);
        }
    }
}

bool svw_view::HasPngWrites()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return !m_pngWrites.empty();
}

void svw_view::FlushPngWrites()
{
    std::vector<PngWrite> writes;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        writes.swap(m_pngWrites);
    }

    for (PngWrite& write : writes)
    {
        sv_status status = SV_OK;
        std::string error;
        if (!svw::WritePngUtf8(write.path.c_str(), write.pixels, write.width, write.height, error))
            status = Fail(SV_ERR_IO, "svw_view_snapshot_png: " + error);

        if (write.completion != nullptr)
        {
            write.completion->status = status;
            SetEvent(write.completion->event);
        }
    }
}

sv_status svw_view::SetSyncScaling(int scaling)
{
    if (scaling != SVW_SYNC_SCALING_FILL && scaling != SVW_SYNC_SCALING_FIT && scaling != SVW_SYNC_SCALING_STRETCH)
        return Fail(SV_ERR_ARG, "svw_view_set_sync_scaling: scaling must be SVW_SYNC_SCALING_FILL, _FIT or _STRETCH");

    m_syncScaling = scaling;
    m_presentPending = true;
    WakeGroup();
    return SV_OK;
}

void svw_view::DetachFromGroup(svw_group* group)
{
    LeaveGroupLocked(group);
}

void svw_view::NotifyEvicted(svw_group* group)
{
    Post([this, group]() {
        bool drop = false;
        {
            std::lock_guard<std::mutex> lock(m_groupMutex);
            if (m_group == group)
            {
                m_group = nullptr;
                drop = true;
            }
        }

        if (m_parkedFor == group)
            Unpark();

        if (drop)
            group->Release();
    });
}

void svw_view::ServeGroupedSnapshots(int width, int height)
{
    std::vector<SnapshotRequest> requests;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        requests.swap(m_snapshots);
    }

    ServeSnapshots(requests, 0, width, height, true);
}

svw_group* svw_view::AcquireGroup()
{
    std::lock_guard<std::mutex> lock(m_groupMutex);
    if (m_group != nullptr)
        m_group->AddRef();

    return m_group;
}

void svw_view::WakeGroup()
{
    const GroupRef group(AcquireGroup());
    if (group.Get() != nullptr)
        group.Get()->Wake();
}

sv_status svw_view::ParkFor(svw_group* group)
{
    if (m_ctx == nullptr)
        return CurrentFailure();

    if (m_parkedFor == group)
        return SV_OK;

    if (!m_ctxPaused)
    {
        sv_pause(m_ctx);
        m_ctxPaused = true;
    }
    svw::WithdrawAudioLevels(m_ctx);

    if (eglMakeCurrent(m_lease.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE)
        return Fail(SV_ERR_GL, svw::DescribeEglError("eglMakeCurrent (park for sync group)", eglGetError()));

    m_parkedFor = group;
    return SV_OK;
}

void svw_view::Unpark()
{
    if (m_parkedFor == nullptr)
        return;

    m_parkedFor = nullptr;
    if (m_ctx == nullptr)
        return;

    if (svw::DisplayStale(m_lease))
    {
        Rebuild();
        m_presentPending = true;
        return;
    }

    if (eglMakeCurrent(m_lease.display, m_surface, m_surface, m_context) != EGL_TRUE)
    {
        const EGLint code = eglGetError();
        if (code == EGL_CONTEXT_LOST)
        {
            Rebuild();
            m_presentPending = true;
            return;
        }
        if (!IsWindow(m_hwnd))
            FailFatal(SV_ERR_ARG, "the window was destroyed");
        else
            FailFatal(SV_ERR_GL, svw::DescribeEglError("eglMakeCurrent (leave sync group)", code));
        return;
    }

    eglSwapInterval(m_lease.display, 1);
    m_presentPending = true;
}

void svw_view::LeaveGroupLocked(svw_group* group)
{
    group->RemoveMember(this);

    bool drop = false;
    {
        std::lock_guard<std::mutex> lock(m_groupMutex);
        if (m_group == group)
        {
            m_group = nullptr;
            drop = true;
        }
    }

    RunSync([this]() {
        Unpark();
        return SV_OK;
    });

    if (drop)
        group->Release();
}
