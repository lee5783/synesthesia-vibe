//
//  SynesthesiaRenderer.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.opengl.GLSurfaceView;
import android.util.Log;

import java.util.concurrent.locks.LockSupport;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class SynesthesiaRenderer implements GLSurfaceView.Renderer {

    private static final String LOG_TAG = "SynesthesiaVibe";
    private static final int EGL_CONTEXT_CLIENT_VERSION = 3;
    private static final double DEFAULT_FRAME_SECONDS = 1.0 / 60.0;
    private static final double NANOS_PER_SECOND = 1.0e9;
    private static final float MIN_MAX_FPS = 1.0f;
    private static final long DEADLINE_TOLERANCE_DIVISOR = 5;
    private static final float DEFAULT_PLAYBACK_RATE = 1.0f;
    private static final float DEFAULT_DEBUG_INTERVAL = 1.0f;

    public interface Listener {
        void onContextCreated(SynesthesiaContext context);

        void onContextError(SynesthesiaException error);
    }

    private final SynesthesiaConfiguration configuration;
    private final Listener listener;
    private final long frameIntervalNanos;
    private volatile SynesthesiaContext context;
    private volatile boolean paused;
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
    private volatile SynesthesiaSyncGroup syncGroup;
    private SynesthesiaSyncGroup appliedSyncGroup;
    private volatile SynesthesiaSyncGroup.Member member;
    private volatile int syncScaling = SyncScaling.FILL;
    private int surfaceWidth;
    private int surfaceHeight;
    private long lastFrameNanos;
    private long nextDeadlineNanos;

    public SynesthesiaRenderer(SynesthesiaConfiguration configuration, Listener listener) {
        this.configuration = configuration;
        this.listener = listener;
        this.frameIntervalNanos = frameIntervalNanos(configuration.getMaxFps());
    }

    public static void configure(GLSurfaceView view) {
        view.setEGLContextClientVersion(EGL_CONTEXT_CLIENT_VERSION);
        view.setPreserveEGLContextOnPause(true);
    }

    public SynesthesiaContext getContext() {
        return context;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        boolean hadContext = context != null;
        releaseLostContext();
        releaseLostMember();
        appliedSyncGroup = null;

        try {
            SynesthesiaContext.initGl();

            if (syncGroup == null || hadContext)
                createContext();
        } catch (SynesthesiaException error) {
            if (listener != null)
                listener.onContextError(error);
        }
    }

    private void createContext() throws SynesthesiaException {
        SynesthesiaContext created = SynesthesiaContext.create(configuration);

        if (paused)
            created.pause();

        appliedPlaybackRate = playbackRate;
        created.setPlaybackRate(appliedPlaybackRate);
        logPlaybackRate(appliedPlaybackRate);

        appliedDebugListener = null;
        appliedDebugInterval = DEFAULT_DEBUG_INTERVAL;
        applyDebug(created);

        appliedTransitionConfiguration = null;
        appliedBlendPattern = BlendPattern.RANDOM;
        appliedPresetRequestListener = null;
        applyTransition(created);

        lastFrameNanos = 0;
        nextDeadlineNanos = 0;
        context = created;

        if (listener != null)
            listener.onContextCreated(created);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        surfaceWidth = width;
        surfaceHeight = height;
        SynesthesiaContext current = context;

        if (current != null)
            current.resize(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        if ((member != null || syncGroup != appliedSyncGroup) && drawGrouped())
            return;

        SynesthesiaContext current = context;

        if (current == null)
            return;

        applyPlaybackRate(current);
        applyDebug(current);
        applyTransition(current);

        if (frameIntervalNanos > 0 && !paused)
            waitForDeadline();

        long now = System.nanoTime();
        double deltaSeconds = lastFrameNanos != 0 ? (now - lastFrameNanos) / NANOS_PER_SECOND : firstFrameSeconds();
        lastFrameNanos = now;
        current.render(deltaSeconds, 0);

        if (frameIntervalNanos > 0)
            advanceDeadline(now);
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

    public SynesthesiaSyncGroup getSyncGroup() {
        return syncGroup;
    }

    public void setSyncGroup(SynesthesiaSyncGroup group) {
        syncGroup = group;
        SynesthesiaSyncGroup.Member grouped = member;

        if (grouped != null && !grouped.belongsTo(group))
            grouped.detach();
    }

    public int getSyncScaling() {
        return syncScaling;
    }

    public void setSyncScaling(int scaling) {
        syncScaling = scaling;
    }

    public void pause() {
        paused = true;
        SynesthesiaSyncGroup.Member grouped = member;

        if (grouped != null)
            grouped.setResumed(false);

        SynesthesiaContext current = context;

        if (current != null)
            current.pause();
    }

    public void resume() {
        paused = false;
        lastFrameNanos = 0;
        nextDeadlineNanos = 0;
        SynesthesiaSyncGroup.Member grouped = member;

        if (grouped != null) {
            grouped.setResumed(true);
            return;
        }

        SynesthesiaContext current = context;

        if (current != null)
            current.resume();
    }

    public void release() {
        SynesthesiaSyncGroup.Member grouped = member;
        member = null;
        appliedSyncGroup = null;

        if (grouped != null)
            grouped.leave(false);

        SynesthesiaContext current = context;
        context = null;

        if (current != null)
            current.close();
    }

    private void releaseLostContext() {
        SynesthesiaContext lost = context;
        context = null;

        if (lost != null)
            lost.closeAfterContextLoss();
    }

    private void releaseLostMember() {
        SynesthesiaSyncGroup.Member lost = member;
        member = null;

        if (lost != null)
            lost.leave(true);
    }

    private boolean drawGrouped() {
        SynesthesiaSyncGroup requested = syncGroup;
        SynesthesiaSyncGroup.Member current = member;

        if (requested != appliedSyncGroup || (current != null && current.isDetached()))
            applySyncGroup(requested);

        SynesthesiaSyncGroup.Member grouped = member;

        if (grouped == null)
            return false;

        int status = grouped.present(surfaceWidth, surfaceHeight, syncScaling);

        if (status == SynesthesiaSyncGroup.PRESENT_DRAWN)
            return true;

        if (status == SynesthesiaSyncGroup.PRESENT_NO_FRAME)
            return grouped.hasPresented() || context == null;

        String error = grouped.lastError();
        leaveGroup(grouped);

        if (status == SynesthesiaSyncGroup.PRESENT_FAILED)
            reportError(new SynesthesiaException(SynesthesiaException.SV_ERR_GL, error));

        resumeOwnContext();
        return false;
    }

    private void applySyncGroup(SynesthesiaSyncGroup requested) {
        appliedSyncGroup = requested;
        SynesthesiaSyncGroup.Member grouped = member;

        if (grouped != null)
            leaveGroup(grouped);

        if (requested == null || !joinGroup(requested))
            resumeOwnContext();
    }

    private boolean joinGroup(SynesthesiaSyncGroup group) {
        SynesthesiaSyncGroup.Member joined;

        try {
            joined = group.join(surfaceWidth, surfaceHeight, !paused);
        } catch (SynesthesiaException error) {
            Log.w(LOG_TAG, "SynesthesiaRenderer sync group join failed on " + Thread.currentThread().getName() + ": " + error.getMessage());
            reportError(error);
            return false;
        }

        member = joined;
        joined.setResumed(!paused);
        SynesthesiaContext current = context;

        if (current != null)
            current.pause();

        Log.i(LOG_TAG, "SynesthesiaRenderer joined sync group on " + Thread.currentThread().getName() + " scaling=" + SyncScaling.name(syncScaling));
        return true;
    }

    private void leaveGroup(SynesthesiaSyncGroup.Member grouped) {
        member = null;
        grouped.leave(false);
        Log.i(LOG_TAG, "SynesthesiaRenderer left sync group on " + Thread.currentThread().getName());
    }

    private void resumeOwnContext() {
        SynesthesiaContext current = context;

        if (current == null) {
            try {
                createContext();
            } catch (SynesthesiaException error) {
                reportError(error);
                return;
            }

            SynesthesiaContext created = context;

            if (surfaceWidth > 0 && surfaceHeight > 0)
                created.resize(surfaceWidth, surfaceHeight);

            return;
        }

        if (paused)
            current.pause();
        else
            current.resume();

        lastFrameNanos = 0;
        nextDeadlineNanos = 0;
    }

    private void reportError(SynesthesiaException error) {
        if (listener != null)
            listener.onContextError(error);
    }

    private void applyPlaybackRate(SynesthesiaContext current) {
        float rate = playbackRate;

        if (Float.compare(rate, appliedPlaybackRate) == 0)
            return;

        appliedPlaybackRate = rate;
        current.setPlaybackRate(rate);
        logPlaybackRate(rate);
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

    private static void logPlaybackRate(float rate) {
        Log.i(LOG_TAG, "SynesthesiaRenderer playback rate " + rate + " applied on " + Thread.currentThread().getName());
    }

    private void waitForDeadline() {
        if (nextDeadlineNanos == 0)
            return;

        long wakeNanos = nextDeadlineNanos - frameIntervalNanos / DEADLINE_TOLERANCE_DIVISOR;
        long remaining = wakeNanos - System.nanoTime();

        while (remaining > 0 && !paused) {
            LockSupport.parkNanos(remaining);
            remaining = wakeNanos - System.nanoTime();
        }
    }

    private void advanceDeadline(long now) {
        nextDeadlineNanos += frameIntervalNanos;

        if (nextDeadlineNanos < now)
            nextDeadlineNanos = now + frameIntervalNanos;
    }

    private static long frameIntervalNanos(float maxFps) {
        if (!(maxFps > 0.0f) || Float.isInfinite(maxFps))
            return 0;

        return Math.round(NANOS_PER_SECOND / Math.max(maxFps, MIN_MAX_FPS));
    }

    private double firstFrameSeconds() {
        float fps = configuration.getFpsHint();
        return fps > 0.0f ? 1.0 / fps : DEFAULT_FRAME_SECONDS;
    }
}
