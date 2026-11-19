//
//  sync_group_jni.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#include <synesthesia-vibe/synesthesia-vibe.h>

#include "gl_headers.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/log.h>
#include <jni.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kLogTag = "SynesthesiaVibe";
    constexpr int kRingSize = 3;
    constexpr int kErrorDrainLimit = 8;
    constexpr EGLTimeKHR kClientWaitNanos = 100000000;
    constexpr jint kPresentNoFrame = 0;
    constexpr jint kPresentDrawn = 1;
    constexpr jint kPresentFailed = -1;
    constexpr jint kPresentClosed = -2;
    constexpr jint kScalingFit = 1;
    constexpr jint kScalingStretch = 2;

    struct SyncProcs
    {
        PFNEGLCREATEIMAGEKHRPROC createImage = nullptr;
        PFNEGLDESTROYIMAGEKHRPROC destroyImage = nullptr;
        PFNEGLCREATESYNCKHRPROC createSync = nullptr;
        PFNEGLDESTROYSYNCKHRPROC destroySync = nullptr;
        PFNEGLCLIENTWAITSYNCKHRPROC clientWaitSync = nullptr;
        PFNEGLWAITSYNCKHRPROC waitSync = nullptr;
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture = nullptr;
    };

    struct MemberState;

    using ReadFence = std::pair<const MemberState*, EGLSyncKHR>;

    struct RingSlot
    {
        GLuint texture = 0;
        GLuint framebuffer = 0;
        EGLImageKHR image = EGL_NO_IMAGE_KHR;
        EGLSyncKHR writeFence = EGL_NO_SYNC_KHR;
        std::vector<ReadFence> readFences;
        int readers = 0;
    };

    struct GroupState
    {
        std::mutex mutex;
        EGLDisplay display = EGL_NO_DISPLAY;
        SyncProcs procs;
        bool transportReady = false;
        bool closed = false;
        RingSlot slots[kRingSize];
        int width = 0;
        int height = 0;
        uint64_t generation = 0;
        int latest = -1;
        int writing = -1;
    };

    struct MemberState
    {
        std::shared_ptr<GroupState> group;
        GLuint textures[kRingSize] = {};
        uint64_t importedGeneration[kRingSize] = {};
        GLuint readFramebuffer = 0;
    };

    struct BlitRects
    {
        GLint sourceX0;
        GLint sourceY0;
        GLint sourceX1;
        GLint sourceY1;
        GLint targetX0;
        GLint targetY0;
        GLint targetX1;
        GLint targetY1;
    };

    std::shared_ptr<GroupState>* ToGroup(jlong handle)
    {
        return reinterpret_cast<std::shared_ptr<GroupState>*>(static_cast<intptr_t>(handle));
    }

    MemberState* ToMember(jlong handle)
    {
        return reinterpret_cast<MemberState*>(static_cast<intptr_t>(handle));
    }

    jbyteArray ToBytes(JNIEnv* env, const std::string& text)
    {
        jbyteArray bytes = env->NewByteArray(static_cast<jsize>(text.size()));

        if (bytes != nullptr && !text.empty())
            env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()), reinterpret_cast<const jbyte*>(text.data()));

        return bytes;
    }

    void ReportError(JNIEnv* env, jobjectArray errorOut, const std::string& error)
    {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "%s", error.c_str());

        if (errorOut == nullptr || env->GetArrayLength(errorOut) < 1)
            return;

        jbyteArray message = ToBytes(env, error);
        env->SetObjectArrayElement(errorOut, 0, message);
        env->DeleteLocalRef(message);
    }

    bool HasExtension(const char* extensions, const char* name)
    {
        if (extensions == nullptr)
            return false;

        const size_t length = std::strlen(name);
        const char* cursor = extensions;

        while ((cursor = std::strstr(cursor, name)) != nullptr)
        {
            const bool startsToken = cursor == extensions || cursor[-1] == ' ';
            const bool endsToken = cursor[length] == ' ' || cursor[length] == '\0';

            if (startsToken && endsToken)
                return true;

            cursor += length;
        }

        return false;
    }

    template <typename Proc>
    Proc ResolveProc(const char* name)
    {
        return reinterpret_cast<Proc>(eglGetProcAddress(name));
    }

    void DrainGlErrors()
    {
        for (int index = 0; index < kErrorDrainLimit && glGetError() != GL_NO_ERROR; ++index)
        {
        }
    }

    void WaitFence(const SyncProcs& procs, EGLDisplay display, EGLSyncKHR fence)
    {
        if (fence == EGL_NO_SYNC_KHR)
            return;

        if (procs.waitSync != nullptr)
            procs.waitSync(display, fence, 0);
        else
            procs.clientWaitSync(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, kClientWaitNanos);
    }

    void ReleaseSlots(GroupState& group, bool contextLost)
    {
        for (RingSlot& slot : group.slots)
        {
            for (const ReadFence& fence : slot.readFences)
                group.procs.destroySync(group.display, fence.second);

            if (slot.writeFence != EGL_NO_SYNC_KHR)
                group.procs.destroySync(group.display, slot.writeFence);

            if (slot.image != EGL_NO_IMAGE_KHR)
                group.procs.destroyImage(group.display, slot.image);

            if (!contextLost)
            {
                if (slot.framebuffer != 0)
                    glDeleteFramebuffers(1, &slot.framebuffer);

                if (slot.texture != 0)
                    glDeleteTextures(1, &slot.texture);
            }

            slot = RingSlot();
        }

        group.width = 0;
        group.height = 0;
        group.latest = -1;
        group.writing = -1;
    }

    bool CreateSlot(GroupState& group, RingSlot& slot, int width, int height, std::string& error)
    {
        glGenTextures(1, &slot.texture);
        glBindTexture(GL_TEXTURE_2D, slot.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &slot.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, slot.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.texture, 0);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (slot.texture == 0 || slot.framebuffer == 0 || status != GL_FRAMEBUFFER_COMPLETE)
        {
            char message[128];
            std::snprintf(message, sizeof(message), "sync group texture %dx%d is incomplete (status 0x%x)", width, height, status);
            error = message;
            return false;
        }

        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const EGLint attributes[] = {EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
        slot.image = group.procs.createImage(group.display, eglGetCurrentContext(), EGL_GL_TEXTURE_2D_KHR,
                                             reinterpret_cast<EGLClientBuffer>(static_cast<intptr_t>(slot.texture)), attributes);

        if (slot.image == EGL_NO_IMAGE_KHR)
        {
            char message[128];
            std::snprintf(message, sizeof(message), "sync group eglCreateImageKHR failed (EGL error 0x%x)", eglGetError());
            error = message;
            return false;
        }

        return true;
    }

    BlitRects ScaledRects(int sourceWidth, int sourceHeight, int targetWidth, int targetHeight, jint scaling)
    {
        BlitRects rects = {0, 0, sourceWidth, sourceHeight, 0, 0, targetWidth, targetHeight};

        if (scaling == kScalingStretch || sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0)
            return rects;

        const double sourceAspect = static_cast<double>(sourceWidth) / sourceHeight;
        const double targetAspect = static_cast<double>(targetWidth) / targetHeight;

        if (scaling == kScalingFit)
        {
            if (sourceAspect > targetAspect)
            {
                const GLint height = static_cast<GLint>(std::lround(targetWidth / sourceAspect));
                rects.targetY0 = (targetHeight - height) / 2;
                rects.targetY1 = rects.targetY0 + height;
            }
            else
            {
                const GLint width = static_cast<GLint>(std::lround(targetHeight * sourceAspect));
                rects.targetX0 = (targetWidth - width) / 2;
                rects.targetX1 = rects.targetX0 + width;
            }

            return rects;
        }

        if (sourceAspect > targetAspect)
        {
            const GLint width = static_cast<GLint>(std::lround(sourceHeight * targetAspect));
            rects.sourceX0 = (sourceWidth - width) / 2;
            rects.sourceX1 = rects.sourceX0 + width;
        }
        else
        {
            const GLint height = static_cast<GLint>(std::lround(sourceWidth / targetAspect));
            rects.sourceY0 = (sourceHeight - height) / 2;
            rects.sourceY1 = rects.sourceY0 + height;
        }

        return rects;
    }

    void ClearView(int width, int height)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void StoreReadFence(GroupState& group, RingSlot& slot, const MemberState* member, EGLSyncKHR fence)
    {
        for (ReadFence& entry : slot.readFences)
        {
            if (entry.first == member)
            {
                group.procs.destroySync(group.display, entry.second);
                entry.second = fence;
                return;
            }
        }

        slot.readFences.emplace_back(member, fence);
    }

    void DropReadFences(GroupState& group, const MemberState* member)
    {
        for (RingSlot& slot : group.slots)
        {
            for (auto entry = slot.readFences.begin(); entry != slot.readFences.end();)
            {
                if (entry->first != member)
                {
                    ++entry;
                    continue;
                }

                group.procs.destroySync(group.display, entry->second);
                entry = slot.readFences.erase(entry);
            }
        }
    }

    bool ImportSlot(MemberState& member, const GroupState& group, int index, std::string& error)
    {
        if (member.textures[index] == 0)
            glGenTextures(1, &member.textures[index]);

        DrainGlErrors();
        glBindTexture(GL_TEXTURE_2D, member.textures[index]);
        group.procs.imageTargetTexture(GL_TEXTURE_2D, static_cast<GLeglImageOES>(group.slots[index].image));
        const GLenum status = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);

        if (member.textures[index] == 0 || status != GL_NO_ERROR)
        {
            char message[128];
            std::snprintf(message, sizeof(message), "sync group glEGLImageTargetTexture2DOES failed (GL error 0x%x)", status);
            error = message;
            return false;
        }

        member.importedGeneration[index] = group.generation;
        return true;
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeCreate(JNIEnv*, jclass)
{
    GroupState* state = new (std::nothrow) GroupState();

    if (state == nullptr)
        return 0;

    std::shared_ptr<GroupState>* holder = new (std::nothrow) std::shared_ptr<GroupState>(state);

    if (holder == nullptr)
    {
        delete state;
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<intptr_t>(holder));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeInitTransport(JNIEnv* env, jclass, jlong handle, jobjectArray errorOut)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr)
    {
        ReportError(env, errorOut, "sync group: no native state");
        return SV_ERR_ARG;
    }

    EGLDisplay display = eglGetCurrentDisplay();

    if (display == EGL_NO_DISPLAY)
    {
        ReportError(env, errorOut, "sync group: no EGL display is current on the group thread");
        return SV_ERR_GL;
    }

    const char* eglExtensions = eglQueryString(display, EGL_EXTENSIONS);
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    const bool imageBase = HasExtension(eglExtensions, "EGL_KHR_image_base");
    const bool textureImage = HasExtension(eglExtensions, "EGL_KHR_gl_texture_2D_image");
    const bool fenceSync = HasExtension(eglExtensions, "EGL_KHR_fence_sync");
    const bool waitSync = HasExtension(eglExtensions, "EGL_KHR_wait_sync");
    const bool oesImage = HasExtension(glExtensions, "GL_OES_EGL_image");

    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "sync group extension check EGL_KHR_image_base=%d EGL_KHR_gl_texture_2D_image=%d EGL_KHR_fence_sync=%d EGL_KHR_wait_sync=%d GL_OES_EGL_image=%d",
                        imageBase, textureImage, fenceSync, waitSync, oesImage);

    if (!imageBase || !textureImage || !fenceSync || !oesImage)
    {
        std::string missing;

        for (const auto& entry : {std::make_pair(imageBase, "EGL_KHR_image_base"), std::make_pair(textureImage, "EGL_KHR_gl_texture_2D_image"),
                                  std::make_pair(fenceSync, "EGL_KHR_fence_sync"), std::make_pair(oesImage, "GL_OES_EGL_image")})
        {
            if (!entry.first)
                missing += missing.empty() ? entry.second : std::string(" ") + entry.second;
        }

        ReportError(env, errorOut, "sync group transport unavailable, missing: " + missing);
        return SV_ERR_GL;
    }

    SyncProcs procs;
    procs.createImage = ResolveProc<PFNEGLCREATEIMAGEKHRPROC>("eglCreateImageKHR");
    procs.destroyImage = ResolveProc<PFNEGLDESTROYIMAGEKHRPROC>("eglDestroyImageKHR");
    procs.createSync = ResolveProc<PFNEGLCREATESYNCKHRPROC>("eglCreateSyncKHR");
    procs.destroySync = ResolveProc<PFNEGLDESTROYSYNCKHRPROC>("eglDestroySyncKHR");
    procs.clientWaitSync = ResolveProc<PFNEGLCLIENTWAITSYNCKHRPROC>("eglClientWaitSyncKHR");
    procs.waitSync = waitSync ? ResolveProc<PFNEGLWAITSYNCKHRPROC>("eglWaitSyncKHR") : nullptr;
    procs.imageTargetTexture = ResolveProc<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>("glEGLImageTargetTexture2DOES");

    if (procs.createImage == nullptr || procs.destroyImage == nullptr || procs.createSync == nullptr || procs.destroySync == nullptr ||
        procs.clientWaitSync == nullptr || procs.imageTargetTexture == nullptr)
    {
        ReportError(env, errorOut, "sync group transport unavailable: an EGLImage or EGL fence entry point did not resolve");
        return SV_ERR_GL;
    }

    GroupState& group = **holder;
    std::lock_guard<std::mutex> lock(group.mutex);
    group.display = display;
    group.procs = procs;
    group.transportReady = true;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "sync group transport ready wait=%s", waitSync ? "server" : "client");
    return SV_OK;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeResize(JNIEnv* env, jclass, jlong handle, jint width, jint height, jobjectArray errorOut)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr || width <= 0 || height <= 0)
    {
        ReportError(env, errorOut, "sync group resize: invalid state or size");
        return SV_ERR_ARG;
    }

    GroupState& group = **holder;
    std::lock_guard<std::mutex> lock(group.mutex);

    if (!group.transportReady)
    {
        ReportError(env, errorOut, "sync group resize: transport is not ready");
        return SV_ERR_GL;
    }

    ReleaseSlots(group, false);
    ++group.generation;
    std::string error;

    for (RingSlot& slot : group.slots)
    {
        if (!CreateSlot(group, slot, width, height, error))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            ReleaseSlots(group, false);
            ReportError(env, errorOut, error);
            return SV_ERR_GL;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    group.width = width;
    group.height = height;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "sync group ring %dx%d generation %llu", width, height,
                        static_cast<unsigned long long>(group.generation));
    return SV_OK;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeBeginFrame(JNIEnv*, jclass, jlong handle)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr)
        return 0;

    GroupState& group = **holder;
    std::vector<ReadFence> pending;
    GLuint framebuffer = 0;

    {
        std::lock_guard<std::mutex> lock(group.mutex);

        if (!group.transportReady || group.closed || group.width == 0)
            return 0;

        for (int offset = 1; offset <= kRingSize; ++offset)
        {
            const int index = ((group.latest + offset) % kRingSize + kRingSize) % kRingSize;
            RingSlot& slot = group.slots[index];

            if (index == group.latest || slot.readers > 0)
                continue;

            if (slot.writeFence != EGL_NO_SYNC_KHR)
            {
                group.procs.destroySync(group.display, slot.writeFence);
                slot.writeFence = EGL_NO_SYNC_KHR;
            }

            pending.swap(slot.readFences);
            group.writing = index;
            framebuffer = slot.framebuffer;
            break;
        }
    }

    for (const ReadFence& fence : pending)
    {
        WaitFence(group.procs, group.display, fence.second);
        group.procs.destroySync(group.display, fence.second);
    }

    return static_cast<jint>(framebuffer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeEndFrame(JNIEnv*, jclass, jlong handle)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr)
        return;

    GroupState& group = **holder;

    if (!group.transportReady)
        return;

    EGLSyncKHR fence = group.procs.createSync(group.display, EGL_SYNC_FENCE_KHR, nullptr);

    if (fence == EGL_NO_SYNC_KHR)
        glFinish();
    else
        glFlush();

    std::lock_guard<std::mutex> lock(group.mutex);

    if (group.writing < 0)
    {
        if (fence != EGL_NO_SYNC_KHR)
            group.procs.destroySync(group.display, fence);

        return;
    }

    group.slots[group.writing].writeFence = fence;
    group.latest = group.writing;
    group.writing = -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeReleaseTransport(JNIEnv*, jclass, jlong handle, jboolean contextLost)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr)
        return;

    GroupState& group = **holder;
    std::lock_guard<std::mutex> lock(group.mutex);

    if (group.transportReady)
        ReleaseSlots(group, contextLost == JNI_TRUE);

    ++group.generation;
    group.transportReady = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeDestroy(JNIEnv*, jclass, jlong handle)
{
    std::shared_ptr<GroupState>* holder = ToGroup(handle);

    if (holder == nullptr)
        return;

    {
        GroupState& group = **holder;
        std::lock_guard<std::mutex> lock(group.mutex);

        if (group.transportReady)
            ReleaseSlots(group, true);

        group.transportReady = false;
        group.closed = true;
    }

    delete holder;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeMemberCreate(JNIEnv* env, jclass, jlong groupHandle, jobjectArray errorOut)
{
    std::shared_ptr<GroupState>* holder = ToGroup(groupHandle);

    if (holder == nullptr)
    {
        ReportError(env, errorOut, "sync group join: the group has no native state");
        return 0;
    }

    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    if (!HasExtension(glExtensions, "GL_OES_EGL_image"))
    {
        ReportError(env, errorOut, "sync group join: this view's GL context lacks GL_OES_EGL_image");
        return 0;
    }

    MemberState* member = new (std::nothrow) MemberState();

    if (member == nullptr)
    {
        ReportError(env, errorOut, "sync group join: out of memory");
        return 0;
    }

    member->group = *holder;
    glGenFramebuffers(1, &member->readFramebuffer);

    if (member->readFramebuffer == 0)
    {
        delete member;
        ReportError(env, errorOut, "sync group join: glGenFramebuffers failed");
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<intptr_t>(member));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeMemberPresent(JNIEnv* env, jclass, jlong handle, jint width, jint height, jint scaling, jobjectArray errorOut)
{
    MemberState* member = ToMember(handle);

    if (member == nullptr)
    {
        ReportError(env, errorOut, "sync group present: the member has no native state");
        return kPresentFailed;
    }

    GroupState& group = *member->group;
    int index = -1;
    uint64_t generation = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    SyncProcs procs;
    EGLDisplay display = EGL_NO_DISPLAY;

    {
        std::lock_guard<std::mutex> lock(group.mutex);

        if (group.closed)
            return kPresentClosed;

        if (group.transportReady && group.latest >= 0)
        {
            index = group.latest;
            std::string error;

            if (member->importedGeneration[index] != group.generation && !ImportSlot(*member, group, index, error))
            {
                ReportError(env, errorOut, error);
                return kPresentFailed;
            }

            RingSlot& slot = group.slots[index];
            WaitFence(group.procs, group.display, slot.writeFence);
            ++slot.readers;
            generation = group.generation;
            sourceWidth = group.width;
            sourceHeight = group.height;
            procs = group.procs;
            display = group.display;
        }
    }

    ClearView(width, height);

    if (index < 0)
        return kPresentNoFrame;

    const BlitRects rects = ScaledRects(sourceWidth, sourceHeight, width, height, scaling);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, member->readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, member->textures[index], 0);
    glBlitFramebuffer(rects.sourceX0, rects.sourceY0, rects.sourceX1, rects.sourceY1,
                      rects.targetX0, rects.targetY0, rects.targetX1, rects.targetY1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    EGLSyncKHR readFence = procs.createSync(display, EGL_SYNC_FENCE_KHR, nullptr);

    if (readFence == EGL_NO_SYNC_KHR)
        glFinish();
    else
        glFlush();

    {
        std::lock_guard<std::mutex> lock(group.mutex);

        if (!group.closed && group.generation == generation)
        {
            RingSlot& slot = group.slots[index];
            --slot.readers;

            if (readFence != EGL_NO_SYNC_KHR)
            {
                StoreReadFence(group, slot, member, readFence);
                readFence = EGL_NO_SYNC_KHR;
            }
        }
    }

    if (readFence != EGL_NO_SYNC_KHR)
        procs.destroySync(display, readFence);

    return kPresentDrawn;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaSyncGroup_nativeMemberDestroy(JNIEnv*, jclass, jlong handle, jboolean contextLost)
{
    MemberState* member = ToMember(handle);

    if (member == nullptr)
        return;

    {
        GroupState& group = *member->group;
        std::lock_guard<std::mutex> lock(group.mutex);

        if (group.transportReady)
            DropReadFences(group, member);
    }

    if (contextLost != JNI_TRUE)
    {
        glDeleteTextures(kRingSize, member->textures);
        glDeleteFramebuffers(1, &member->readFramebuffer);
    }

    delete member;
}
