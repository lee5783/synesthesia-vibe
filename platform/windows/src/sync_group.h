//
//  sync_group.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_SYNC_GROUP_H__
#define __SYNESTHESIA_VIBE_WINDOWS_SYNC_GROUP_H__ 1

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

namespace svw
{

std::mutex& MembershipMutex();

}

struct svw_group
{
public:
    static sv_status Create(const svw_group_config& config, const sv_config* engine, svw_group*& outGroup, std::string& error);

    ~svw_group();

    svw_group(const svw_group&) = delete;
    svw_group& operator=(const svw_group&) = delete;

    void AddRef();
    void Release();
    void Close();

    sv_status LoadPreset(const char* path, float blendSeconds);
    sv_status LoadPresetMemory(const char* text, size_t len, float blendSeconds);
    sv_status Pause();
    sv_status Resume();
    sv_status SetPlaybackRate(float rate);
    sv_status SetPreferredFps(float fps);
    sv_status SetRenderScale(float scale);
    sv_status SetFixedRenderSize(int width, int height);
    sv_status GetRenderSize(int& width, int& height);
    sv_status GetMemberCount(int& count);
    sv_status Invoke(svw_invoke_fn fn, void* user);

    sv_status AddMember(svw_view* view);
    sv_status RemoveMember(svw_view* view);
    std::vector<svw_view*> Members();

    int Adapter() const { return m_adapter; }
    bool OnRenderThread() const;
    void Wake();
    std::string LastError() const;
    sv_status Fail(sv_status status, const std::string& message);

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

    struct Member
    {
        svw_view* view;
        HWND hwnd;
        EGLSurface surface;
        DPI_AWARENESS_CONTEXT awareness;
        int width;
        int height;
        bool presented;
    };

    svw_group(const svw_group_config& config, const sv_config* engine);

    sv_status RunSync(std::function<sv_status()> task);
    void Post(std::function<void()> task);
    sv_status CurrentFailure();
    void FailFatal(sv_status status, const std::string& message);

    void ThreadMain();
    void RunThread();
    void RunLoop();
    void DrainCommands();
    bool ReleaseGlTurn();
    void TakeGlTurn(bool ownsCurrent);
    void WaitForWork(bool withTimer);
    void FailPendingWork();

    bool BuildGraphics(std::string& error);
    void TearDownGraphics();
    bool MakeOwnCurrent();

    void PruneDeadMembers();
    void RejectUnpresentableSnapshots();
    void WritePendingPngs(const std::vector<svw_view*>& views);
    bool AnyMemberActive() const;
    bool AnyMemberNeedsPresent() const;
    bool ApplyRunState();
    void ApplyRenderSize();
    void RenderAndPresent(double dtSeconds);
    bool PresentMember(Member& member);
    void BlitToMember(const Member& member, int scaling);
    void EvictMember(size_t index);
    void FlushEvictions();

    int m_adapter;
    sv_config m_engine;
    std::string m_resourcePath;
    std::string m_textureSearchPath;
    std::string m_cachePath;

    std::atomic<int> m_refs;
    std::thread m_thread;
    std::atomic<DWORD> m_threadId;
    HANDLE m_wake;
    HANDLE m_ready;
    sv_status m_createStatus;
    std::string m_createError;

    std::mutex m_queueMutex;
    std::vector<Command> m_commands;
    bool m_threadExited;

    mutable std::mutex m_errorMutex;
    std::string m_lastError;

    std::atomic<bool> m_stopping;
    std::atomic<bool> m_pauseRequested;
    std::atomic<float> m_renderScale;
    std::atomic<float> m_preferredFps;
    std::atomic<float> m_playbackRate;
    std::atomic<int> m_fixedWidth;
    std::atomic<int> m_fixedHeight;
    std::atomic<int> m_renderWidth;
    std::atomic<int> m_renderHeight;
    std::atomic<int> m_memberCount;

    svw::FramePacer m_pacer;
    svw::DisplayLease m_lease;
    EGLContext m_context;
    EGLSurface m_ownSurface;
    sv_context* m_ctx;
    render_gl::Framebuffer m_frame;
    std::unique_lock<std::mutex>* m_glLock;

    std::vector<Member> m_members;
    std::vector<svw_view*> m_evicted;
    int m_grownWidth;
    int m_grownHeight;
    bool m_ctxPaused;
    bool m_presentPending;
    bool m_failed;
    sv_status m_failStatus;
    std::string m_failMessage;
};

#endif
