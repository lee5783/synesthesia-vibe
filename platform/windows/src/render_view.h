//
//  render_view.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_RENDER_VIEW_H__
#define __SYNESTHESIA_VIBE_WINDOWS_RENDER_VIEW_H__ 1

#include <synesthesia-vibe/synesthesia-vibe-windows.h>

#include "egl_display.h"
#include "frame_pacer.h"
#include "framebuffer.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct svw_view
{
public:
    static sv_status Create(const svw_view_config& config, const sv_config* engine, svw_view*& outView, std::string& error);
    ~svw_view();

    svw_view(const svw_view&) = delete;
    svw_view& operator=(const svw_view&) = delete;

    sv_status LoadPreset(const char* path, float blendSeconds);
    sv_status LoadPresetMemory(const char* text, size_t len, float blendSeconds);
    sv_status Pause();
    sv_status Resume();
    sv_status SetPlaybackRate(float rate);
    sv_status SetPreferredFps(float fps);
    sv_status SetRenderScale(float scale);
    sv_status Snapshot(svw_snapshot_fn fn, void* user);
    sv_status SnapshotPng(const char* path);
    sv_status Invoke(svw_invoke_fn fn, void* user);
    sv_status SetSyncGroup(svw_group* group);
    sv_status SetSyncScaling(int scaling);

    void DetachFromGroup(svw_group* group);
    void NotifyEvicted(svw_group* group);
    HWND Window() const { return m_hwnd; }
    EGLSurface Surface() const { return m_surface; }
    bool PauseRequested() const { return m_pauseRequested.load(); }
    int SyncScaling() const { return m_syncScaling.load(); }
    bool PresentPending() const { return m_presentPending.load(); }
    bool TakePresentPending() { return m_presentPending.exchange(false); }
    bool HasPendingSnapshots();
    void ServeGroupedSnapshots(int width, int height);
    void RejectPendingSnapshots(const char* reason);
    bool HasPngWrites();
    void FlushPngWrites();

    bool OnRenderThread() const;
    std::string LastError() const;
    sv_status Fail(sv_status status, const std::string& message);

    void NoteWindowMoved(HWND hwnd);

private:
    struct Completion
    {
        HANDLE event;
        sv_status status;
    };

    struct Command
    {
        std::function<sv_status()> run;
        Completion* completion;
    };

    struct SnapshotRequest
    {
        svw_snapshot_fn fn;
        void* user;
        std::string pngPath;
        Completion* completion;
    };

    struct PngWrite
    {
        std::vector<uint8_t> pixels;
        int width;
        int height;
        std::string path;
        Completion* completion;
    };

    svw_view(const svw_view_config& config, const sv_config* engine);

    sv_status RunSync(std::function<sv_status()> task);
    void Post(std::function<void()> task);
    sv_status Enqueue(SnapshotRequest request);
    sv_status CurrentFailure();
    void FailFatal(sv_status status, const std::string& message);

    void ThreadMain();
    void RunThread();
    void RunLoop();
    void PumpMessages();
    void DrainCommands();
    bool ReleaseGlTurn();
    void TakeGlTurn(bool ownsCurrent);
    void WaitForWork(bool withTimer);
    void WritePendingPngs();
    void FailPendingWork();
    void FailPendingSnapshots();

    bool BuildGraphics(std::string& error);
    void TearDownGraphics();
    void Rebuild();
    void ReloadLastPreset();

    bool ApplyPauseState();
    bool UpdateGeometry();
    uint32_t CurrentTarget() const;
    void RenderAndPresent(double dtSeconds);
    void BlitToWindow();
    void ServeSnapshots(std::vector<SnapshotRequest>& requests, uint32_t target, int width, int height, bool deferPngWrites);

    svw_group* AcquireGroup();
    void WakeGroup();
    sv_status ParkFor(svw_group* group);
    void Unpark();
    void LeaveGroupLocked(svw_group* group);

    HWND m_hwnd;
    int m_adapter;
    sv_config m_engine;
    std::string m_resourcePath;
    std::string m_textureSearchPath;
    std::string m_cachePath;

    std::thread m_thread;
    std::atomic<DWORD> m_threadId;
    HANDLE m_wake;
    HANDLE m_ready;
    sv_status m_createStatus;
    std::string m_createError;

    std::mutex m_queueMutex;
    std::vector<Command> m_commands;
    std::vector<SnapshotRequest> m_snapshots;
    std::vector<PngWrite> m_pngWrites;
    bool m_threadExited;

    mutable std::mutex m_errorMutex;
    std::string m_lastError;

    std::atomic<bool> m_stopping;
    std::atomic<bool> m_pauseRequested;
    std::atomic<float> m_renderScale;
    std::atomic<float> m_preferredFps;
    std::atomic<float> m_playbackRate;

    svw::FramePacer m_pacer;
    svw::DisplayLease m_lease;
    EGLSurface m_surface;
    EGLContext m_context;
    sv_context* m_ctx;
    render_gl::Framebuffer m_scaled;
    HWINEVENTHOOK m_winEventHook;
    std::unique_lock<std::mutex>* m_glLock;

    int m_surfaceWidth;
    int m_surfaceHeight;
    int m_backingWidth;
    int m_backingHeight;
    bool m_ctxPaused;
    std::atomic<bool> m_presentPending;
    bool m_failed;
    sv_status m_failStatus;
    std::string m_failMessage;

    std::string m_lastPresetPath;
    std::string m_lastPresetText;
    bool m_lastPresetFromMemory;

    std::mutex m_groupMutex;
    svw_group* m_group;
    svw_group* m_parkedFor;
    std::atomic<int> m_syncScaling;
};

#endif
