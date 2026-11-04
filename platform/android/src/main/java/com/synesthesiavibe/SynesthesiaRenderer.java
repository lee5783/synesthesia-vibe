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
    private volatile int slowMotion = SynesthesiaContext.SLOW_MOTION_INTERPOLATE;
    private int appliedSlowMotion = SynesthesiaContext.SLOW_MOTION_INTERPOLATE;
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
        releaseLostContext();

        try {
            SynesthesiaContext.initGl();
            SynesthesiaContext created = SynesthesiaContext.create(configuration);

            if (paused)
                created.pause();

            appliedPlaybackRate = playbackRate;
            created.setPlaybackRate(appliedPlaybackRate);
            logPlaybackRate(appliedPlaybackRate);

            appliedSlowMotion = slowMotion;
            created.setSlowMotion(appliedSlowMotion);
            logSlowMotion(appliedSlowMotion);

            lastFrameNanos = 0;
            nextDeadlineNanos = 0;
            context = created;

            if (listener != null)
                listener.onContextCreated(created);
        } catch (SynesthesiaException error) {
            if (listener != null)
                listener.onContextError(error);
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        SynesthesiaContext current = context;

        if (current != null)
            current.resize(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        SynesthesiaContext current = context;

        if (current == null)
            return;

        applyPlaybackRate(current);
        applySlowMotion(current);

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

    public int getSlowMotion() {
        return slowMotion;
    }

    public void setSlowMotion(int mode) {
        slowMotion = SynesthesiaContext.normalizeSlowMotion(mode);
    }

    public void pause() {
        paused = true;
        SynesthesiaContext current = context;

        if (current != null)
            current.pause();
    }

    public void resume() {
        paused = false;
        lastFrameNanos = 0;
        nextDeadlineNanos = 0;
        SynesthesiaContext current = context;

        if (current != null)
            current.resume();
    }

    public void release() {
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

    private void applyPlaybackRate(SynesthesiaContext current) {
        float rate = playbackRate;

        if (Float.compare(rate, appliedPlaybackRate) == 0)
            return;

        appliedPlaybackRate = rate;
        current.setPlaybackRate(rate);
        logPlaybackRate(rate);
    }

    private void applySlowMotion(SynesthesiaContext current) {
        int mode = slowMotion;

        if (mode == appliedSlowMotion)
            return;

        appliedSlowMotion = mode;
        current.setSlowMotion(mode);
        logSlowMotion(mode);
    }

    private static void logSlowMotion(int mode) {
        Log.i(LOG_TAG, "SynesthesiaRenderer slow motion " + mode + " applied on " + Thread.currentThread().getName());
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
