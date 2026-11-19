//
//  SynesthesiaSyncGroup.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

package com.synesthesiavibe;

import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.EGLSurface;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Choreographer;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.CopyOnWriteArrayList;

public final class SynesthesiaSyncGroup implements AutoCloseable {

    static final int PRESENT_NO_FRAME = 0;
    static final int PRESENT_DRAWN = 1;
    static final int PRESENT_FAILED = -1;
    static final int PRESENT_CLOSED = -2;

    private static final String LOG_TAG = "SynesthesiaVibe";
    private static final String THREAD_NAME = "SynesthesiaSyncGroup";
    private static final int EGL_CONTEXT_CLIENT_VERSION = 3;
    private static final int COLOR_CHANNEL_BITS = 8;
    private static final int PBUFFER_PIXELS = 1;
    private static final double DEFAULT_FRAME_SECONDS = 1.0 / 60.0;
    private static final double NANOS_PER_SECOND = 1.0e9;
    private static final float MIN_MAX_FPS = 1.0f;
    private static final long DEADLINE_TOLERANCE_DIVISOR = 5;
    private static final float DEFAULT_PLAYBACK_RATE = 1.0f;
    private static final float DEFAULT_DEBUG_INTERVAL = 1.0f;
    private static final float DEFAULT_RENDER_SCALE = 1.0f;
    private static final float MIN_RENDER_SCALE = 0.25f;

    static {
        System.loadLibrary("synesthesiavibe-jni");
    }

    private final SynesthesiaConfiguration configuration;
    private final SynesthesiaRenderer.Listener listener;
    private final Object membershipLock = new Object();
    private final CopyOnWriteArrayList<Member> members = new CopyOnWriteArrayList<>();
    private final Choreographer.FrameCallback frameCallback = this::onFrame;
    private final HandlerThread thread;
    private final Handler handler;
    private long nativeHandle;
    private volatile boolean closed;
    private volatile SynesthesiaException failure;
    private volatile SynesthesiaContext context;
    private volatile float playbackRate = DEFAULT_PLAYBACK_RATE;
    private float appliedPlaybackRate = DEFAULT_PLAYBACK_RATE;
    private volatile SynesthesiaContext.DebugListener debugListener;
    private volatile float debugInterval = DEFAULT_DEBUG_INTERVAL;
    private SynesthesiaContext.DebugListener appliedDebugListener;
    private float appliedDebugInterval = DEFAULT_DEBUG_INTERVAL;
    private volatile TransitionConfiguration transitionConfiguration;
    private TransitionConfiguration appliedTransitionConfiguration;
    private volatile int blendPattern = BlendPattern.RANDOM;
    private int appliedBlendPattern = BlendPattern.RANDOM;
    private volatile SynesthesiaContext.PresetRequestListener presetRequestListener;
    private SynesthesiaContext.PresetRequestListener appliedPresetRequestListener;
    private volatile float renderScale = DEFAULT_RENDER_SCALE;
    private volatile Size renderSize;
    private volatile float maxFps;
    private Choreographer choreographer;
    private EGLDisplay eglDisplay = EGL14.EGL_NO_DISPLAY;
    private EGLContext eglContext = EGL14.EGL_NO_CONTEXT;
    private EGLSurface eglSurface = EGL14.EGL_NO_SURFACE;
    private boolean running;
    private int renderWidth;
    private int renderHeight;
    private int grownWidth;
    private int grownHeight;
    private long lastFrameNanos;

    public SynesthesiaSyncGroup(SynesthesiaConfiguration configuration, SynesthesiaRenderer.Listener listener) throws SynesthesiaException {
        if (configuration.isDeterministic())
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "a sync group refuses a deterministic configuration");

        long created = nativeCreate();

        if (created == 0)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "sync group native state could not be allocated");

        this.configuration = configuration;
        this.listener = listener;
        this.maxFps = configuration.getMaxFps();
        this.nativeHandle = created;
        this.thread = new HandlerThread(THREAD_NAME);
        this.thread.start();
        this.handler = new Handler(thread.getLooper());
        handler.post(this::start);
    }

    public SynesthesiaContext getContext() {
        return context;
    }

    public boolean queueEvent(Runnable event) {
        if (closed)
            return false;

        return handler.post(() -> {
            if (!closed && failure == null)
                event.run();
        });
    }

    public float getPlaybackRate() {
        return playbackRate;
    }

    public void setPlaybackRate(float rate) {
        playbackRate = rate;
    }

    public SynesthesiaContext.DebugListener getDebugListener() {
        return debugListener;
    }

    public void setDebugListener(SynesthesiaContext.DebugListener listener) {
        debugListener = listener;
    }

    public float getDebugInterval() {
        return debugInterval;
    }

    public void setDebugInterval(float seconds) {
        debugInterval = seconds;
    }

    public TransitionConfiguration getTransitionConfiguration() {
        return transitionConfiguration;
    }

    public void setTransitionConfiguration(TransitionConfiguration configuration) {
        transitionConfiguration = configuration;
    }

    public int getBlendPattern() {
        return blendPattern;
    }

    public void setBlendPattern(int pattern) {
        blendPattern = pattern;
    }

    public SynesthesiaContext.PresetRequestListener getPresetRequestListener() {
        return presetRequestListener;
    }

    public void setPresetRequestListener(SynesthesiaContext.PresetRequestListener listener) {
        presetRequestListener = listener;
    }

    public float getRenderScale() {
        return renderScale;
    }

    public void setRenderScale(float scale) {
        renderScale = Float.isFinite(scale) ? Math.min(Math.max(scale, MIN_RENDER_SCALE), DEFAULT_RENDER_SCALE) : DEFAULT_RENDER_SCALE;
    }

    public Size getRenderSize() {
        return renderSize;
    }

    public void setRenderSize(int widthPixels, int heightPixels) {
        renderSize = widthPixels > 0 && heightPixels > 0 ? new Size(widthPixels, heightPixels) : null;
    }

    public float getMaxFps() {
        return maxFps;
    }

    public void setMaxFps(float fps) {
        maxFps = fps;
    }

    public boolean isClosed() {
        return closed;
    }

    @Override
    public void close() {
        synchronized (membershipLock) {
            if (closed)
                return;

            closed = true;
        }

        handler.post(this::shutdown);
        thread.quitSafely();
    }

    Member join(int widthPixels, int heightPixels, boolean resumed) throws SynesthesiaException {
        SynesthesiaException failed = failure;

        if (failed != null)
            throw new SynesthesiaException(failed.getStatusCode(), failed.getMessage());

        byte[][] error = new byte[1][];
        Member member;

        synchronized (membershipLock) {
            if (closed || nativeHandle == 0)
                throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "the sync group is closed");

            long handle = nativeMemberCreate(nativeHandle, error);

            if (handle == 0)
                throw new SynesthesiaException(SynesthesiaException.SV_ERR_GL, decode(error[0]));

            member = new Member(this, handle, widthPixels, heightPixels, resumed);
            members.add(member);
        }

        Log.i(LOG_TAG, "SynesthesiaSyncGroup member joined " + widthPixels + "x" + heightPixels + " on " + Thread.currentThread().getName() + " members=" + members.size());
        requestWake();
        return member;
    }

    private void start() {
        choreographer = Choreographer.getInstance();

        try {
            createEgl();
            createContext();
        } catch (SynesthesiaException error) {
            fail(error);
            return;
        }

        Log.i(LOG_TAG, "SynesthesiaSyncGroup started on " + Thread.currentThread().getName());
        wake();
    }

    private void createEgl() throws SynesthesiaException {
        eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        int[] version = new int[2];

        if (eglDisplay.equals(EGL14.EGL_NO_DISPLAY) || !EGL14.eglInitialize(eglDisplay, version, 0, version, 1))
            throw eglFailure("eglInitialize");

        int[] configAttributes = {
            EGL14.EGL_RENDERABLE_TYPE, EGLExt.EGL_OPENGL_ES3_BIT_KHR,
            EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
            EGL14.EGL_RED_SIZE, COLOR_CHANNEL_BITS,
            EGL14.EGL_GREEN_SIZE, COLOR_CHANNEL_BITS,
            EGL14.EGL_BLUE_SIZE, COLOR_CHANNEL_BITS,
            EGL14.EGL_ALPHA_SIZE, COLOR_CHANNEL_BITS,
            EGL14.EGL_NONE
        };
        EGLConfig[] configs = new EGLConfig[1];
        int[] count = new int[1];

        if (!EGL14.eglChooseConfig(eglDisplay, configAttributes, 0, configs, 0, configs.length, count, 0) || count[0] == 0)
            throw eglFailure("eglChooseConfig");

        int[] contextAttributes = {EGL14.EGL_CONTEXT_CLIENT_VERSION, EGL_CONTEXT_CLIENT_VERSION, EGL14.EGL_NONE};
        eglContext = EGL14.eglCreateContext(eglDisplay, configs[0], EGL14.EGL_NO_CONTEXT, contextAttributes, 0);

        if (eglContext.equals(EGL14.EGL_NO_CONTEXT))
            throw eglFailure("eglCreateContext");

        int[] surfaceAttributes = {EGL14.EGL_WIDTH, PBUFFER_PIXELS, EGL14.EGL_HEIGHT, PBUFFER_PIXELS, EGL14.EGL_NONE};
        eglSurface = EGL14.eglCreatePbufferSurface(eglDisplay, configs[0], surfaceAttributes, 0);

        if (eglSurface.equals(EGL14.EGL_NO_SURFACE))
            throw eglFailure("eglCreatePbufferSurface");

        if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
            throw eglFailure("eglMakeCurrent");
    }

    private void createContext() throws SynesthesiaException {
        SynesthesiaContext.initGl();
        byte[][] error = new byte[1][];
        int status = nativeInitTransport(nativeHandle, error);

        if (status != SynesthesiaException.SV_OK)
            throw new SynesthesiaException(status, decode(error[0]));

        SynesthesiaContext created = SynesthesiaContext.create(configuration);
        created.pause();

        appliedPlaybackRate = DEFAULT_PLAYBACK_RATE;
        appliedDebugListener = null;
        appliedDebugInterval = DEFAULT_DEBUG_INTERVAL;
        appliedTransitionConfiguration = null;
        appliedBlendPattern = BlendPattern.RANDOM;
        appliedPresetRequestListener = null;
        applyPlaybackRate(created);
        applyDebug(created);
        applyTransition(created);

        running = false;
        renderWidth = 0;
        renderHeight = 0;
        lastFrameNanos = 0;
        context = created;

        if (listener != null)
            listener.onContextCreated(created);
    }

    private void requestWake() {
        if (!closed)
            handler.post(this::wake);
    }

    private void wake() {
        if (closed || failure != null || context == null)
            return;

        boolean shouldRun = anyMemberResumed();

        if (shouldRun == running)
            return;

        if (shouldRun)
            startRunning();
        else
            stopRunning();
    }

    private void startRunning() {
        if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            if (EGL14.eglGetError() != EGL14.EGL_CONTEXT_LOST) {
                fail(eglFailure("eglMakeCurrent"));
                return;
            }

            if (!recreateAfterContextLoss())
                return;
        }

        SynesthesiaContext current = context;
        current.resume();
        lastFrameNanos = 0;
        running = true;
        choreographer.postFrameCallback(frameCallback);
        Log.i(LOG_TAG, "SynesthesiaSyncGroup running members=" + members.size());
    }

    private void stopRunning() {
        running = false;
        choreographer.removeFrameCallback(frameCallback);
        SynesthesiaContext current = context;

        if (current != null)
            current.pause();

        Log.i(LOG_TAG, "SynesthesiaSyncGroup paused, no member is resumed");
    }

    private boolean recreateAfterContextLoss() {
        Log.w(LOG_TAG, "SynesthesiaSyncGroup EGL context lost, recreating the group context");
        SynesthesiaContext lost = context;
        context = null;
        nativeReleaseTransport(nativeHandle, true);

        if (lost != null)
            lost.closeAfterContextLoss();

        destroyEgl();

        try {
            createEgl();
            createContext();
        } catch (SynesthesiaException error) {
            fail(error);
            return false;
        }

        return true;
    }

    private void onFrame(long frameTimeNanos) {
        if (!running)
            return;

        choreographer.postFrameCallback(frameCallback);
        long interval = frameIntervalNanos(maxFps);

        if (interval > 0 && lastFrameNanos != 0 && frameTimeNanos - lastFrameNanos < interval - interval / DEADLINE_TOLERANCE_DIVISOR)
            return;

        SynesthesiaContext current = context;

        if (current == null)
            return;

        applyPlaybackRate(current);
        applyDebug(current);
        applyTransition(current);

        if (!ensureRenderSize(current))
            return;

        int framebuffer = nativeBeginFrame(nativeHandle);

        if (framebuffer == 0)
            return;

        double deltaSeconds = lastFrameNanos != 0 ? (frameTimeNanos - lastFrameNanos) / NANOS_PER_SECOND : firstFrameSeconds();
        lastFrameNanos = frameTimeNanos;
        current.render(deltaSeconds, framebuffer);
        nativeEndFrame(nativeHandle);
    }

    private boolean ensureRenderSize(SynesthesiaContext current) {
        Size fixed = renderSize;
        int width;
        int height;

        if (fixed != null) {
            width = fixed.getWidth();
            height = fixed.getHeight();
        } else {
            float scale = renderScale;

            for (Member member : members) {
                grownWidth = Math.max(grownWidth, member.width);
                grownHeight = Math.max(grownHeight, member.height);
            }

            width = grownWidth > 0 ? Math.max(1, Math.round(grownWidth * scale)) : 0;
            height = grownHeight > 0 ? Math.max(1, Math.round(grownHeight * scale)) : 0;
        }

        if (width <= 0 || height <= 0)
            return false;

        if (width == renderWidth && height == renderHeight)
            return true;

        byte[][] error = new byte[1][];
        int status = nativeResize(nativeHandle, width, height, error);

        if (status != SynesthesiaException.SV_OK) {
            fail(new SynesthesiaException(status, decode(error[0])));
            return false;
        }

        current.resize(width, height);
        renderWidth = width;
        renderHeight = height;
        Log.i(LOG_TAG, "SynesthesiaSyncGroup render size " + width + "x" + height);
        return true;
    }

    private boolean anyMemberResumed() {
        for (Member member : members) {
            if (member.resumed)
                return true;
        }

        return false;
    }

    private void fail(SynesthesiaException error) {
        failure = error;
        Log.e(LOG_TAG, "SynesthesiaSyncGroup failed: " + error.getMessage());
        releaseRendering();

        if (listener != null)
            listener.onContextError(error);
    }

    private void shutdown() {
        releaseRendering();

        synchronized (membershipLock) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }

        Log.i(LOG_TAG, "SynesthesiaSyncGroup closed");
    }

    private void releaseRendering() {
        running = false;

        if (choreographer != null)
            choreographer.removeFrameCallback(frameCallback);

        nativeReleaseTransport(nativeHandle, eglContext.equals(EGL14.EGL_NO_CONTEXT));
        SynesthesiaContext current = context;
        context = null;

        if (current != null)
            current.close();

        destroyEgl();
    }

    private void destroyEgl() {
        if (!eglDisplay.equals(EGL14.EGL_NO_DISPLAY)) {
            EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);

            if (!eglSurface.equals(EGL14.EGL_NO_SURFACE))
                EGL14.eglDestroySurface(eglDisplay, eglSurface);

            if (!eglContext.equals(EGL14.EGL_NO_CONTEXT))
                EGL14.eglDestroyContext(eglDisplay, eglContext);
        }

        eglSurface = EGL14.EGL_NO_SURFACE;
        eglContext = EGL14.EGL_NO_CONTEXT;
    }

    private void applyPlaybackRate(SynesthesiaContext current) {
        float rate = playbackRate;

        if (Float.compare(rate, appliedPlaybackRate) == 0)
            return;

        appliedPlaybackRate = rate;
        current.setPlaybackRate(rate);
    }

    private void applyDebug(SynesthesiaContext current) {
        SynesthesiaContext.DebugListener listener = debugListener;
        float interval = debugInterval;

        if (Float.compare(interval, appliedDebugInterval) != 0) {
            appliedDebugInterval = interval;
            current.setDebugInterval(interval);
        }

        if (listener != appliedDebugListener) {
            appliedDebugListener = listener;
            current.setDebugListener(listener);
        }
    }

    private void applyTransition(SynesthesiaContext current) {
        TransitionConfiguration configuration = transitionConfiguration;
        int pattern = blendPattern;
        SynesthesiaContext.PresetRequestListener listener = presetRequestListener;

        if (configuration != appliedTransitionConfiguration) {
            appliedTransitionConfiguration = configuration;
            current.setTransitionConfiguration(configuration);
        }

        if (pattern != appliedBlendPattern) {
            appliedBlendPattern = pattern;
            current.setBlendPattern(pattern);
        }

        if (listener != appliedPresetRequestListener) {
            appliedPresetRequestListener = listener;
            current.setPresetRequestListener(listener);
        }
    }

    private double firstFrameSeconds() {
        float fps = configuration.getFpsHint();
        return fps > 0.0f ? 1.0 / fps : DEFAULT_FRAME_SECONDS;
    }

    private static long frameIntervalNanos(float maxFps) {
        if (!(maxFps > 0.0f) || Float.isInfinite(maxFps))
            return 0;

        return Math.round(NANOS_PER_SECOND / Math.max(maxFps, MIN_MAX_FPS));
    }

    private static SynesthesiaException eglFailure(String call) {
        return new SynesthesiaException(SynesthesiaException.SV_ERR_GL, "sync group " + call + " failed (EGL error 0x" + Integer.toHexString(EGL14.eglGetError()) + ")");
    }

    private static String decode(byte[] bytes) {
        return bytes != null ? new String(bytes, StandardCharsets.UTF_8) : "";
    }

    static final class Member {

        private final SynesthesiaSyncGroup group;
        private final byte[][] error = new byte[1][];
        private long handle;
        private volatile int width;
        private volatile int height;
        private volatile boolean resumed;
        private volatile boolean detached;
        private boolean presented;
        private String lastError = "";

        private Member(SynesthesiaSyncGroup group, long handle, int width, int height, boolean resumed) {
            this.group = group;
            this.handle = handle;
            this.width = width;
            this.height = height;
            this.resumed = resumed;
        }

        int present(int widthPixels, int heightPixels, int scaling) {
            width = widthPixels;
            height = heightPixels;

            if (group.closed || handle == 0)
                return PRESENT_CLOSED;

            SynesthesiaException failed = group.failure;

            if (failed != null) {
                lastError = failed.getMessage();
                return PRESENT_FAILED;
            }

            int status = nativeMemberPresent(handle, widthPixels, heightPixels, scaling, error);

            if (status == PRESENT_DRAWN)
                presented = true;
            else if (status == PRESENT_FAILED)
                lastError = decode(error[0]);

            return status;
        }

        String lastError() {
            return lastError;
        }

        boolean hasPresented() {
            return presented;
        }

        boolean belongsTo(SynesthesiaSyncGroup candidate) {
            return group == candidate;
        }

        boolean isDetached() {
            return detached;
        }

        void detach() {
            boolean removed;

            synchronized (group.membershipLock) {
                detached = true;
                removed = group.members.remove(this);
            }

            if (!removed)
                return;

            Log.i(LOG_TAG, "SynesthesiaSyncGroup member detached on " + Thread.currentThread().getName() + " members=" + group.members.size());
            group.requestWake();
        }

        void setResumed(boolean value) {
            if (resumed == value)
                return;

            resumed = value;
            group.requestWake();
        }

        void leave(boolean contextLost) {
            synchronized (group.membershipLock) {
                detached = true;
                group.members.remove(this);
            }

            long released = handle;
            handle = 0;

            if (released != 0)
                nativeMemberDestroy(released, contextLost);

            Log.i(LOG_TAG, "SynesthesiaSyncGroup member left on " + Thread.currentThread().getName() + " contextLost=" + contextLost + " members=" + group.members.size());
            group.requestWake();
        }
    }

    private static native long nativeCreate();

    private static native int nativeInitTransport(long handle, byte[][] errorOut);

    private static native int nativeResize(long handle, int width, int height, byte[][] errorOut);

    private static native int nativeBeginFrame(long handle);

    private static native void nativeEndFrame(long handle);

    private static native void nativeReleaseTransport(long handle, boolean contextLost);

    private static native void nativeDestroy(long handle);

    private static native long nativeMemberCreate(long groupHandle, byte[][] errorOut);

    private static native int nativeMemberPresent(long handle, int width, int height, int scaling, byte[][] errorOut);

    private static native void nativeMemberDestroy(long handle, boolean contextLost);
}
