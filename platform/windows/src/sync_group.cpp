//
//  sync_group.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#include "sync_group.h"

#include "windows_headers.h"
#include "gl_headers.h"

#include "audio_router.h"
#include "host_support.h"
#include "render_view.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <utility>

namespace
{
    constexpr int kPlaceholderWidth = 1280;
    constexpr int kPlaceholderHeight = 720;

    std::mutex g_membershipMutex;

    struct BlitRects
    {
        GLint source[4];
        GLint target[4];
    };

    BlitRects BlitRectsForScaling(int sourceWidth, int sourceHeight, int targetWidth, int targetHeight, int scaling)
    {
        BlitRects rects = { { 0, 0, sourceWidth, sourceHeight }, { 0, 0, targetWidth, targetHeight } };
        if (scaling == SVW_SYNC_SCALING_STRETCH || sourceWidth < 1 || sourceHeight < 1 || targetWidth < 1 || targetHeight < 1)
            return rects;

        const double sourceAspect = static_cast<double>(sourceWidth) / sourceHeight;
        const double targetAspect = static_cast<double>(targetWidth) / targetHeight;

        if (scaling == SVW_SYNC_SCALING_FIT)
        {
            if (sourceAspect > targetAspect)
            {
                const GLint height = static_cast<GLint>(std::lround(targetWidth / sourceAspect));
                const GLint y = (targetHeight - height) / 2;
                rects.target[1] = y;
                rects.target[3] = y + height;
            }
            else
            {
                const GLint width = static_cast<GLint>(std::lround(targetHeight * sourceAspect));
                const GLint x = (targetWidth - width) / 2;
                rects.target[0] = x;
                rects.target[2] = x + width;
            }
            return rects;
        }

        if (sourceAspect > targetAspect)
        {
            const GLint width = static_cast<GLint>(std::lround(sourceHeight * targetAspect));
            const GLint x = (sourceWidth - width) / 2;
            rects.source[0] = x;
            rects.source[2] = x + width;
        }
        else
        {
            const GLint height = static_cast<GLint>(std::lround(sourceWidth / targetAspect));
            const GLint y = (sourceHeight - height) / 2;
            rects.source[1] = y;
            rects.source[3] = y + height;
        }
        return rects;
    }
}

namespace svw
{

std::mutex& MembershipMutex()
{
    return g_membershipMutex;
}

}

sv_status svw_group::Create(const svw_group_config& config, const sv_config* engine, svw_group*& outGroup, std::string& error)
{
    outGroup = nullptr;

    if (config.adapter != SVW_ADAPTER_DEFAULT && config.adapter != SVW_ADAPTER_WARP)
    {
        error = "adapter must be SVW_ADAPTER_DEFAULT or SVW_ADAPTER_WARP";
        return SV_ERR_ARG;
    }

    if (engine != nullptr && engine->deterministic)
    {
        error = "a sync group refuses a deterministic configuration";
        return SV_ERR_ARG;
    }

    std::unique_ptr<svw_group> group(new svw_group(config, engine));
    group->m_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    group->m_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (group->m_wake == nullptr || group->m_ready == nullptr || !group->m_pacer.Valid())
    {
        error = "could not create the group thread's events or timer";
        return SV_ERR_GL;
    }

    group->m_thread = std::thread(&svw_group::ThreadMain, group.get());
    svw::WaitServicingSentMessages(group->m_ready);

    if (group->m_createStatus != SV_OK)
    {
        error = group->m_createError;
        group->m_thread.join();
        return group->m_createStatus;
    }

    outGroup = group.release();
    return SV_OK;
}

svw_group::svw_group(const svw_group_config& config, const sv_config* engine)
    : m_adapter(config.adapter)
    , m_engine()
    , m_refs(1)
    , m_threadId(0)
    , m_wake(nullptr)
    , m_ready(nullptr)
    , m_createStatus(SV_ERR_GL)
    , m_threadExited(false)
    , m_stopping(false)
    , m_pauseRequested(config.start_paused)
    , m_renderScale(svw::kMaxRenderScale)
    , m_preferredFps(0.0f)
    , m_playbackRate(1.0f)
    , m_fixedWidth(0)
    , m_fixedHeight(0)
    , m_renderWidth(0)
    , m_renderHeight(0)
    , m_memberCount(0)
    , m_context(EGL_NO_CONTEXT)
    , m_ownSurface(EGL_NO_SURFACE)
    , m_ctx(nullptr)
    , m_glLock(nullptr)
    , m_grownWidth(0)
    , m_grownHeight(0)
    , m_ctxPaused(false)
    , m_presentPending(false)
    , m_failed(false)
    , m_failStatus(SV_OK)
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

svw_group::~svw_group()
{
    if (m_thread.joinable())
    {
        m_stopping = true;
        if (m_wake != nullptr)
            SetEvent(m_wake);

        svw::WaitServicingSentMessages(static_cast<HANDLE>(m_thread.native_handle()));
        m_thread.join();
    }

    if (m_wake != nullptr)
        CloseHandle(m_wake);
    if (m_ready != nullptr)
        CloseHandle(m_ready);
}

void svw_group::AddRef()
{
    m_refs.fetch_add(1);
}

void svw_group::Release()
{
    if (m_refs.fetch_sub(1) == 1)
        delete this;
}

void svw_group::Close()
{
    {
        std::lock_guard<std::mutex> lock(svw::MembershipMutex());
        for (svw_view* view : Members())
            view->DetachFromGroup(this);
    }

    if (m_thread.joinable())
    {
        m_stopping = true;
        SetEvent(m_wake);
        svw::WaitServicingSentMessages(static_cast<HANDLE>(m_thread.native_handle()));
        m_thread.join();
    }

    Release();
}

sv_status svw_group::LoadPreset(const char* path, float blendSeconds)
{
    if (path == nullptr || path[0] == '\0')
        return Fail(SV_ERR_ARG, "svw_group_load_preset: path is empty");

    const std::string presetPath(path);
    return RunSync([this, presetPath, blendSeconds]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        const sv_status status = sv_load_preset(m_ctx, presetPath.c_str(), blendSeconds);
        if (status != SV_OK)
            return Fail(status, svw::ErrorText(m_ctx, "sv_load_preset failed"));

        m_presentPending = true;
        return SV_OK;
    });
}

sv_status svw_group::LoadPresetMemory(const char* text, size_t len, float blendSeconds)
{
    if (text == nullptr || len == 0)
        return Fail(SV_ERR_ARG, "svw_group_load_preset_memory: text is empty");

    const std::string presetText(text, len);
    return RunSync([this, presetText, blendSeconds]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        const sv_status status = sv_load_preset_memory(m_ctx, presetText.data(), presetText.size(), blendSeconds);
        if (status != SV_OK)
            return Fail(status, svw::ErrorText(m_ctx, "sv_load_preset_memory failed"));

        m_presentPending = true;
        return SV_OK;
    });
}

sv_status svw_group::Pause()
{
    m_pauseRequested = true;
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_group::Resume()
{
    m_pauseRequested = false;
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_group::SetPlaybackRate(float rate)
{
    if (!std::isfinite(rate))
        return Fail(SV_ERR_ARG, "svw_group_set_playback_rate: rate is not finite");

    m_playbackRate = rate;
    Post([this, rate]() {
        if (m_ctx != nullptr)
            sv_set_playback_rate(m_ctx, rate);
    });
    return SV_OK;
}

sv_status svw_group::SetPreferredFps(float fps)
{
    if (std::isnan(fps))
        return Fail(SV_ERR_ARG, "svw_group_set_preferred_fps: fps is NaN");

    m_preferredFps = std::isfinite(fps) && fps > 0.0f ? fps : 0.0f;
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_group::SetRenderScale(float scale)
{
    if (!std::isfinite(scale))
        return Fail(SV_ERR_ARG, "svw_group_set_render_scale: scale is not finite");

    m_renderScale = svw::ClampRenderScale(scale);
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_group::SetFixedRenderSize(int width, int height)
{
    if (width < 0 || height < 0 || (width == 0) != (height == 0))
        return Fail(SV_ERR_ARG, "svw_group_set_fixed_render_size: width and height must both be positive, or both 0");

    m_fixedWidth = width;
    m_fixedHeight = height;
    SetEvent(m_wake);
    return SV_OK;
}

sv_status svw_group::GetRenderSize(int& width, int& height)
{
    return RunSync([this, &width, &height]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        ApplyRenderSize();
        width = m_renderWidth.load();
        height = m_renderHeight.load();
        return SV_OK;
    });
}

sv_status svw_group::GetMemberCount(int& count)
{
    count = m_memberCount.load();
    return SV_OK;
}

sv_status svw_group::Invoke(svw_invoke_fn fn, void* user)
{
    if (fn == nullptr)
        return Fail(SV_ERR_ARG, "svw_group_invoke: fn is null");

    return RunSync([this, fn, user]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        fn(m_ctx, m_frame.Id(), user);
        m_presentPending = true;
        return SV_OK;
    });
}

sv_status svw_group::AddMember(svw_view* view)
{
    return RunSync([this, view]() {
        if (m_ctx == nullptr)
            return CurrentFailure();

        for (const Member& member : m_members)
        {
            if (member.view == view)
                return SV_OK;
        }

        const EGLSurface surface = view->Surface();
        if (surface == EGL_NO_SURFACE)
            return Fail(SV_ERR_GL, "the view has no window surface");

        Member member = { view, view->Window(), surface, GetWindowDpiAwarenessContext(view->Window()), 0, 0, false };
        EGLint width = 0;
        EGLint height = 0;
        eglQuerySurface(m_lease.display, surface, EGL_WIDTH, &width);
        eglQuerySurface(m_lease.display, surface, EGL_HEIGHT, &height);
        member.width = width;
        member.height = height;

        m_members.push_back(member);
        m_memberCount = static_cast<int>(m_members.size());
        m_presentPending = true;
        return SV_OK;
    });
}

sv_status svw_group::RemoveMember(svw_view* view)
{
    return RunSync([this, view]() {
        m_members.erase(std::remove_if(m_members.begin(), m_members.end(), [view](const Member& member) { return member.view == view; }),
                        m_members.end());
        m_evicted.erase(std::remove(m_evicted.begin(), m_evicted.end(), view), m_evicted.end());
        m_memberCount = static_cast<int>(m_members.size());
        return SV_OK;
    });
}

std::vector<svw_view*> svw_group::Members()
{
    std::vector<svw_view*> views;
    RunSync([this, &views]() {
        for (const Member& member : m_members)
            views.push_back(member.view);
        return SV_OK;
    });
    return views;
}

bool svw_group::OnRenderThread() const
{
    return GetCurrentThreadId() == m_threadId.load();
}

void svw_group::Wake()
{
    SetEvent(m_wake);
}

std::string svw_group::LastError() const
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

sv_status svw_group::Fail(sv_status status, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = message;
    return status;
}

sv_status svw_group::RunSync(std::function<sv_status()> task)
{
    if (OnRenderThread())
        return task();

    svw::ScopedEvent done;
    if (done.Get() == nullptr)
        return Fail(SV_ERR_GL, "could not create a completion event");

    Completion completion{ done.Get(), SV_ERR_GL };
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_threadExited)
            return Fail(SV_ERR_GL, "the group's render thread has stopped");

        m_commands.push_back(Command{ std::move(task), &completion });
    }

    SetEvent(m_wake);
    svw::WaitServicingSentMessages(done.Get());
    return completion.status;
}

void svw_group::Post(std::function<void()> task)
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

sv_status svw_group::CurrentFailure()
{
    if (m_failed)
        return Fail(m_failStatus, m_failMessage);

    return Fail(SV_ERR_GL, "the group has no GL context");
}

void svw_group::FailFatal(sv_status status, const std::string& message)
{
    m_failed = true;
    m_failStatus = status;
    m_failMessage = message;
    Fail(status, message);

    while (!m_members.empty())
        EvictMember(m_members.size() - 1);

    TearDownGraphics();
    FlushEvictions();
}

void svw_group::ThreadMain()
{
    volatile char* headroom = static_cast<volatile char*>(_alloca(svw::kCoreStackHeadroomBytes));
    headroom[0] = 0;
    headroom[svw::kCoreStackHeadroomBytes - 1] = 0;
    RunThread();
}

void svw_group::RunThread()
{
    m_threadId = GetCurrentThreadId();
    svw::MarkHostRenderThread(true);
    SetThreadDescription(GetCurrentThread(), L"synesthesia-vibe sync group");
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

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
        m_createStatus = SV_ERR_GL;
        FailPendingWork();
        svw::MarkHostRenderThread(false);
        SetEvent(m_ready);
        return;
    }

    m_createStatus = SV_OK;
    SetEvent(m_ready);

    RunLoop();

    while (!m_members.empty())
        EvictMember(m_members.size() - 1);

    TearDownGraphics();
    svw::ReleaseDisplay(m_lease);
    eglReleaseThread();
    FlushEvictions();
    FailPendingWork();
    m_glLock = nullptr;
    svw::MarkHostRenderThread(false);
}

void svw_group::RunLoop()
{
    double deadline = 0.0;
    double lastFrameAt = 0.0;

    for (;;)
    {
        if (m_ctx != nullptr && svw::DisplayStale(m_lease))
            FailFatal(SV_ERR_GL, "the sync group's GL context was lost");

        DrainCommands();
        if (m_stopping)
            return;

        if (m_ctx == nullptr)
        {
            FlushEvictions();
            WaitForWork(false);
            continue;
        }

        PruneDeadMembers();
        FlushEvictions();
        RejectUnpresentableSnapshots();

        const double now = m_pacer.Now();
        m_pacer.SetPreferredFps(m_preferredFps.load());
        std::vector<HWND> windows;
        for (const Member& member : m_members)
            windows.push_back(member.hwnd);
        if (!windows.empty() && m_pacer.RefreshDue(windows, now))
        {
            const bool ownsCurrent = ReleaseGlTurn();
            const bool refreshChanged = m_pacer.TrackMonitors(windows, now);
            TakeGlTurn(ownsCurrent);
            if (refreshChanged)
                deadline = now;
            continue;
        }

        if (ApplyRunState())
        {
            lastFrameAt = 0.0;
            deadline = now;
        }

        ApplyRenderSize();

        if (m_ctxPaused)
        {
            if (!m_members.empty() && (m_presentPending || AnyMemberNeedsPresent()))
            {
                m_presentPending = false;
                RenderAndPresent(0.0);
                FlushEvictions();
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
        m_presentPending = false;
        RenderAndPresent(dtSeconds);
        FlushEvictions();
    }
}

void svw_group::DrainCommands()
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

bool svw_group::ReleaseGlTurn()
{
    const bool ownsCurrent = m_context != EGL_NO_CONTEXT;
    if (ownsCurrent)
        eglMakeCurrent(m_lease.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_glLock != nullptr)
        m_glLock->unlock();
    return ownsCurrent;
}

void svw_group::TakeGlTurn(bool ownsCurrent)
{
    if (m_glLock != nullptr)
        m_glLock->lock();
    if (ownsCurrent && m_context != EGL_NO_CONTEXT && !svw::DisplayStale(m_lease))
        MakeOwnCurrent();
}

void svw_group::WaitForWork(bool withTimer)
{
    const HANDLE handles[2] = { m_wake, m_pacer.Timer() };
    const bool ownsCurrent = ReleaseGlTurn();
    WaitForMultipleObjectsEx(withTimer ? 2 : 1, handles, FALSE, INFINITE, FALSE);
    TakeGlTurn(ownsCurrent);
}

void svw_group::FailPendingWork()
{
    std::vector<Command> commands;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_threadExited = true;
        commands.swap(m_commands);
    }

    if (m_createStatus == SV_OK)
        Fail(SV_ERR_GL, m_failed ? m_failMessage : std::string("the group's render thread has stopped"));

    for (Command& command : commands)
    {
        if (command.completion != nullptr)
        {
            command.completion->status = SV_ERR_GL;
            SetEvent(command.completion->event);
        }
    }
}

bool svw_group::BuildGraphics(std::string& error)
{
    m_context = svw::CreateRenderContext(m_lease, error);
    if (m_context == EGL_NO_CONTEXT)
        return false;

    if (!MakeOwnCurrent())
    {
        const EGLint pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
        m_ownSurface = eglCreatePbufferSurface(m_lease.display, m_lease.config, pbufferAttributes);
        if (m_ownSurface == EGL_NO_SURFACE || !MakeOwnCurrent())
        {
            error = svw::DescribeEglError("eglMakeCurrent (sync group, no surface)", eglGetError());
            return false;
        }
    }

    if (!svw::LoadGlFunctionsOnce(error))
        return false;

    const int width = m_engine.width > 0 ? m_engine.width : kPlaceholderWidth;
    const int height = m_engine.height > 0 ? m_engine.height : kPlaceholderHeight;

    sv_config config = m_engine;
    config.width = width;
    config.height = height;
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

    const std::string createError = svw::ErrorText(m_ctx, "");
    if (createError.compare(0, strlen(svw::kRendererUnavailablePrefix), svw::kRendererUnavailablePrefix) == 0)
    {
        error = createError;
        return false;
    }

    if (!m_frame.Create(width, height))
    {
        error = "could not create the group's framebuffer";
        return false;
    }

    m_renderWidth = width;
    m_renderHeight = height;
    svw::RegisterAudioContext(m_ctx);
    sv_set_playback_rate(m_ctx, m_playbackRate.load());
    sv_pause(m_ctx);
    m_ctxPaused = true;
    return true;
}

void svw_group::TearDownGraphics()
{
    const EGLDisplay display = m_lease.display;
    const bool handlesLive = display != EGL_NO_DISPLAY && !svw::DisplayStale(m_lease);

    if (handlesLive && m_context != EGL_NO_CONTEXT)
        MakeOwnCurrent();

    m_frame.Destroy();

    if (m_ctx != nullptr)
    {
        svw::UnregisterAudioContext(m_ctx);
        sv_destroy(m_ctx);
        m_ctx = nullptr;
    }

    if (display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (handlesLive && m_ownSurface != EGL_NO_SURFACE)
            eglDestroySurface(display, m_ownSurface);
        if (handlesLive && m_context != EGL_NO_CONTEXT)
            eglDestroyContext(display, m_context);
    }

    m_ownSurface = EGL_NO_SURFACE;
    m_context = EGL_NO_CONTEXT;
}

bool svw_group::MakeOwnCurrent()
{
    return eglMakeCurrent(m_lease.display, m_ownSurface, m_ownSurface, m_context) == EGL_TRUE;
}

void svw_group::PruneDeadMembers()
{
    for (size_t index = m_members.size(); index-- > 0;)
    {
        if (!IsWindow(m_members[index].hwnd))
            EvictMember(index);
    }
}

bool svw_group::AnyMemberActive() const
{
    for (const Member& member : m_members)
    {
        if (!member.view->PauseRequested() && !IsIconic(member.hwnd))
            return true;
    }
    return false;
}

bool svw_group::AnyMemberNeedsPresent() const
{
    for (const Member& member : m_members)
    {
        if (svw::UnpresentableReason(member.hwnd) != nullptr)
            continue;
        if (!member.presented || member.view->PresentPending() || member.view->HasPendingSnapshots())
            return true;
    }
    return false;
}

void svw_group::RejectUnpresentableSnapshots()
{
    for (const Member& member : m_members)
    {
        const char* reason = svw::UnpresentableReason(member.hwnd);
        if (reason != nullptr && member.view->HasPendingSnapshots())
            member.view->RejectPendingSnapshots(reason);
    }
}

void svw_group::WritePendingPngs(const std::vector<svw_view*>& views)
{
    bool pending = false;
    for (svw_view* view : views)
        pending = pending || view->HasPngWrites();
    if (!pending)
        return;

    const bool ownsCurrent = ReleaseGlTurn();
    for (svw_view* view : views)
        view->FlushPngWrites();
    TakeGlTurn(ownsCurrent);
}

bool svw_group::ApplyRunState()
{
    const bool wantPaused = m_pauseRequested.load() || !AnyMemberActive();
    if (wantPaused == m_ctxPaused)
        return false;

    m_ctxPaused = wantPaused;
    if (wantPaused)
    {
        sv_pause(m_ctx);
        svw::WithdrawAudioLevels(m_ctx);
        return false;
    }

    sv_resume(m_ctx);
    return true;
}

void svw_group::ApplyRenderSize()
{
    for (Member& member : m_members)
    {
        EGLint width = 0;
        EGLint height = 0;
        if (eglQuerySurface(m_lease.display, member.surface, EGL_WIDTH, &width) == EGL_TRUE
            && eglQuerySurface(m_lease.display, member.surface, EGL_HEIGHT, &height) == EGL_TRUE)
        {
            member.width = width;
            member.height = height;
        }

        m_grownWidth = std::max(m_grownWidth, member.width);
        m_grownHeight = std::max(m_grownHeight, member.height);
    }

    int width = m_renderWidth.load();
    int height = m_renderHeight.load();
    const int fixedWidth = m_fixedWidth.load();
    const int fixedHeight = m_fixedHeight.load();
    if (fixedWidth > 0 && fixedHeight > 0)
    {
        width = fixedWidth;
        height = fixedHeight;
    }
    else if (m_grownWidth > 0 && m_grownHeight > 0)
    {
        const float scale = m_renderScale.load();
        width = std::max(1, static_cast<int>(std::floor(m_grownWidth * scale)));
        height = std::max(1, static_cast<int>(std::floor(m_grownHeight * scale)));
    }

    if (width == m_renderWidth.load() && height == m_renderHeight.load())
        return;

    if (!m_frame.Resize(width, height))
    {
        Fail(SV_ERR_GL, "could not resize the group's framebuffer");
        return;
    }

    sv_resize(m_ctx, width, height);
    m_renderWidth = width;
    m_renderHeight = height;
    m_presentPending = true;
}

void svw_group::RenderAndPresent(double dtSeconds)
{
    sv_render_frame(m_ctx, dtSeconds, m_frame.Id());
    if (!m_ctxPaused)
        svw::PublishAudioLevels(m_ctx);

    std::vector<svw_view*> views;
    for (const Member& member : m_members)
        views.push_back(member.view);

    bool contextLost = false;
    for (size_t index = 0; index < m_members.size();)
    {
        Member& member = m_members[index];
        const bool pending = member.view->TakePresentPending();
        const bool skip = svw::UnpresentableReason(member.hwnd) != nullptr
            || (member.presented && member.view->PauseRequested() && !pending && !member.view->HasPendingSnapshots());
        if (skip)
        {
            ++index;
            continue;
        }

        if (PresentMember(member))
        {
            ++index;
            continue;
        }

        if (eglGetError() == EGL_CONTEXT_LOST)
        {
            contextLost = true;
            break;
        }

        EvictMember(index);
    }

    MakeOwnCurrent();
    WritePendingPngs(views);

    if (contextLost)
        FailFatal(SV_ERR_GL, "the sync group's GL context was lost");
}

bool svw_group::PresentMember(Member& member)
{
    if (member.awareness != nullptr)
        SetThreadDpiAwarenessContext(member.awareness);

    if (eglMakeCurrent(m_lease.display, member.surface, member.surface, m_context) != EGL_TRUE)
        return false;

    eglSwapInterval(m_lease.display, 0);

    EGLint width = 0;
    EGLint height = 0;
    eglQuerySurface(m_lease.display, member.surface, EGL_WIDTH, &width);
    eglQuerySurface(m_lease.display, member.surface, EGL_HEIGHT, &height);
    if (width <= 0 || height <= 0)
    {
        eglGetError();
        return true;
    }

    member.width = width;
    member.height = height;
    BlitToMember(member, member.view->SyncScaling());

    if (member.view->HasPendingSnapshots())
        member.view->ServeGroupedSnapshots(width, height);

    if (eglSwapBuffers(m_lease.display, member.surface) != EGL_TRUE)
        return false;

    member.presented = true;
    return true;
}

void svw_group::BlitToMember(const Member& member, int scaling)
{
    const BlitRects rects = BlitRectsForScaling(m_renderWidth.load(), m_renderHeight.load(), member.width, member.height, scaling);

    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    if (scissorEnabled)
        glDisable(GL_SCISSOR_TEST);

    GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_frame.Id());
    glBlitFramebuffer(rects.source[0], rects.source[1], rects.source[2], rects.source[3],
                      rects.target[0], rects.target[1], rects.target[2], rects.target[3], GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (scissorEnabled)
        glEnable(GL_SCISSOR_TEST);
}

void svw_group::EvictMember(size_t index)
{
    m_evicted.push_back(m_members[index].view);
    m_members.erase(m_members.begin() + static_cast<std::ptrdiff_t>(index));
    m_memberCount = static_cast<int>(m_members.size());
}

void svw_group::FlushEvictions()
{
    std::vector<svw_view*> evicted;
    evicted.swap(m_evicted);
    for (svw_view* view : evicted)
        view->NotifyEvicted(this);
}
