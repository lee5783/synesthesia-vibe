//
//  SynesthesiaRenderer.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.opengl.GLSurfaceView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class SynesthesiaRenderer implements GLSurfaceView.Renderer {

    private static final int EGL_CONTEXT_CLIENT_VERSION = 3;
    private static final double DEFAULT_FRAME_SECONDS = 1.0 / 60.0;
    private static final double NANOS_PER_SECOND = 1.0e9;

    public interface Listener {
        void onContextCreated(SynesthesiaContext context);

        void onContextError(SynesthesiaException error);
    }

    private final SynesthesiaConfiguration configuration;
    private final Listener listener;
    private volatile SynesthesiaContext context;
    private volatile boolean paused;
    private long lastFrameNanos;

    public SynesthesiaRenderer(SynesthesiaConfiguration configuration, Listener listener) {
        this.configuration = configuration;
        this.listener = listener;
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

            lastFrameNanos = 0;
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

        long now = System.nanoTime();
        double deltaSeconds = lastFrameNanos != 0 ? (now - lastFrameNanos) / NANOS_PER_SECOND : firstFrameSeconds();
        lastFrameNanos = now;
        current.render(deltaSeconds, 0);
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

    private double firstFrameSeconds() {
        float fps = configuration.getFpsHint();
        return fps > 0.0f ? 1.0 / fps : DEFAULT_FRAME_SECONDS;
    }
}
