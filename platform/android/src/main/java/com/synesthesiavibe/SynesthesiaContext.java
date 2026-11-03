//
//  SynesthesiaContext.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.locks.ReentrantReadWriteLock;

public final class SynesthesiaContext implements AutoCloseable {

    private static final String RENDERER_UNAVAILABLE_PREFIX = "renderer unavailable";
    private static final String VALIDATE_TEMP_PREFIX = "sv-validate-";
    private static final String VALIDATE_TEMP_SUFFIX = ".milk";

    static {
        System.loadLibrary("synesthesiavibe-jni");
    }

    private final ReentrantReadWriteLock lock = new ReentrantReadWriteLock();
    private long handle;

    private SynesthesiaContext(long handle) {
        this.handle = handle;
    }

    public static void initGl() throws SynesthesiaException {
        if (EGL14.eglGetCurrentContext().equals(EGL14.EGL_NO_CONTEXT))
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_GL, "no EGL context is current on this thread");

        int status = nativeInitGl();

        if (status != SynesthesiaException.SV_OK)
            throw new SynesthesiaException(status, "sv_init_gl could not load the OpenGL ES 3.0 entry points");
    }

    public static String glStrings() {
        return decode(nativeGlStrings());
    }

    public static void validatePreset(File preset) throws SynesthesiaException {
        if (preset == null)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "sv_validate_preset: path is empty");

        byte[][] error = new byte[1][];
        int status = nativeValidatePreset(encodePath(preset), error);

        if (status != SynesthesiaException.SV_OK)
            throw new SynesthesiaException(status, decode(error[0]));
    }

    public static void validatePresetText(String text) throws SynesthesiaException {
        validatePresetText(text, null);
    }

    public static void validatePresetText(String text, File temporaryDirectory) throws SynesthesiaException {
        if (text == null)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "sv_validate_preset_memory: text is null");

        File temporary;
        try {
            temporary = File.createTempFile(VALIDATE_TEMP_PREFIX, VALIDATE_TEMP_SUFFIX, temporaryDirectory);
        } catch (IOException | SecurityException error) {
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO,
                    "could not create a temporary preset file in " + describeDirectory(temporaryDirectory) + ": " + error.getMessage());
        }

        try {
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                output.write(text.getBytes(StandardCharsets.UTF_8));
            } catch (IOException error) {
                throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO,
                        "could not write the temporary preset file " + temporary.getPath() + ": " + error.getMessage());
            }

            validatePreset(temporary);
        } finally {
            temporary.delete();
        }
    }

    public static SynesthesiaContext create(SynesthesiaConfiguration configuration) throws SynesthesiaException {
        float[] date = configuration.getPinnedDate();
        long handle = nativeCreate(
                configuration.getWidth(), configuration.getHeight(),
                configuration.getMeshX(), configuration.getMeshY(),
                configuration.getTexsizeX(), configuration.getTexsizeY(),
                configuration.getFpsHint(),
                encodePath(configuration.getResourcePath()),
                encodePath(configuration.getTextureSearchPath()),
                encodePath(configuration.getCachePath()),
                configuration.getRngSeed(),
                configuration.isDeterministic(),
                configuration.isDatePinned(), date,
                configuration.isMousePinned(), configuration.getMouseX(), configuration.getMouseY(),
                configuration.getMaxFps(), configuration.getMaxLuminance(), configuration.getFlashLimit());

        if (handle == 0)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "sv_create returned no context");

        SynesthesiaContext context = new SynesthesiaContext(handle);
        String error = context.lastError();

        if (error.startsWith(RENDERER_UNAVAILABLE_PREFIX)) {
            context.close();
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_GL, error);
        }

        return context;
    }

    public void loadPreset(File preset) throws SynesthesiaException {
        loadPreset(preset, 0.0f);
    }

    public void loadPreset(File preset, float blendSeconds) throws SynesthesiaException {
        lock.readLock().lock();
        try {
            check(nativeLoadPreset(requireHandle(), encodePath(preset), blendSeconds));
        } finally {
            lock.readLock().unlock();
        }
    }

    public void loadPresetText(String text, float blendSeconds) throws SynesthesiaException {
        byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
        lock.readLock().lock();
        try {
            check(nativeLoadPresetText(requireHandle(), bytes, blendSeconds));
        } finally {
            lock.readLock().unlock();
        }
    }

    public void pushPcm(float[] interleaved, int channels) {
        pushPcm(interleaved, interleaved.length / requireChannels(channels), channels);
    }

    public void pushPcm(float[] interleaved, int frames, int channels) {
        requireSamples(interleaved.length, frames, channels);
        lock.readLock().lock();
        try {
            nativePushPcmFloat(requireHandle(), interleaved, frames, channels);
        } finally {
            lock.readLock().unlock();
        }
    }

    public void pushPcm(short[] interleaved, int channels) {
        pushPcm(interleaved, interleaved.length / requireChannels(channels), channels);
    }

    public void pushPcm(short[] interleaved, int frames, int channels) {
        requireSamples(interleaved.length, frames, channels);
        lock.readLock().lock();
        try {
            nativePushPcmShort(requireHandle(), interleaved, frames, channels);
        } finally {
            lock.readLock().unlock();
        }
    }

    public void render(double deltaSeconds, int targetFramebuffer) {
        lock.readLock().lock();
        try {
            nativeRender(requireHandle(), deltaSeconds, targetFramebuffer);
        } finally {
            lock.readLock().unlock();
        }
    }

    public void resize(int widthPixels, int heightPixels) {
        lock.readLock().lock();
        try {
            nativeResize(requireHandle(), widthPixels, heightPixels);
        } finally {
            lock.readLock().unlock();
        }
    }

    public void pause() {
        lock.readLock().lock();
        try {
            nativePause(requireHandle());
        } finally {
            lock.readLock().unlock();
        }
    }

    public void resume() {
        lock.readLock().lock();
        try {
            nativeResume(requireHandle());
        } finally {
            lock.readLock().unlock();
        }
    }

    public void setPlaybackRate(float rate) {
        lock.readLock().lock();
        try {
            nativeSetPlaybackRate(requireHandle(), rate);
        } finally {
            lock.readLock().unlock();
        }
    }

    public AudioLevels audioLevels() {
        float[] levels = new float[3];
        lock.readLock().lock();
        try {
            nativeGetAudioLevels(requireHandle(), levels);
        } finally {
            lock.readLock().unlock();
        }
        return new AudioLevels(levels[0], levels[1], levels[2]);
    }

    public String lastError() {
        lock.readLock().lock();
        try {
            return decode(nativeLastError(requireHandle()));
        } finally {
            lock.readLock().unlock();
        }
    }

    public void dumpFrameState(File json) {
        lock.readLock().lock();
        try {
            nativeDumpFrameState(requireHandle(), encodePath(json));
        } finally {
            lock.readLock().unlock();
        }
    }

    public boolean isClosed() {
        lock.readLock().lock();
        try {
            return handle == 0;
        } finally {
            lock.readLock().unlock();
        }
    }

    @Override
    public void close() {
        lock.writeLock().lock();
        try {
            if (handle != 0) {
                nativeDestroy(handle);
                handle = 0;
            }
        } finally {
            lock.writeLock().unlock();
        }
    }

    void closeAfterContextLoss() {
        EGLContext current = EGL14.eglGetCurrentContext();

        if (current.equals(EGL14.EGL_NO_CONTEXT)) {
            close();
            return;
        }

        EGLDisplay display = EGL14.eglGetCurrentDisplay();
        EGLSurface draw = EGL14.eglGetCurrentSurface(EGL14.EGL_DRAW);
        EGLSurface read = EGL14.eglGetCurrentSurface(EGL14.EGL_READ);
        EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
        try {
            close();
        } finally {
            EGL14.eglMakeCurrent(display, draw, read, current);
        }
    }

    void captureFrames(File wav, int[] frames, File outputDirectory, boolean writesFrameState) throws SynesthesiaException {
        byte[][] error = new byte[1][];
        int status;
        lock.readLock().lock();
        try {
            status = nativeCapture(requireHandle(), encodePath(wav), frames, encodePath(outputDirectory), writesFrameState, error);
        } finally {
            lock.readLock().unlock();
        }

        if (status != SynesthesiaException.SV_OK)
            throw new SynesthesiaException(status, decode(error[0]));
    }

    private void check(int status) throws SynesthesiaException {
        if (status != SynesthesiaException.SV_OK)
            throw new SynesthesiaException(status, decode(nativeLastError(handle)));
    }

    private long requireHandle() {
        if (handle == 0)
            throw new IllegalStateException("SynesthesiaContext is closed");
        return handle;
    }

    private static int requireChannels(int channels) {
        if (channels <= 0)
            throw new IllegalArgumentException("channels must be positive");
        return channels;
    }

    private static void requireSamples(int length, int frames, int channels) {
        requireChannels(channels);
        if (frames < 0 || (long) frames * channels > length)
            throw new IllegalArgumentException("frames * channels exceeds the buffer length");
    }

    private static String describeDirectory(File directory) {
        return directory != null ? directory.getPath() : System.getProperty("java.io.tmpdir", "the default temporary directory");
    }

    private static byte[] encodePath(File file) {
        return file != null ? file.getPath().getBytes(StandardCharsets.UTF_8) : null;
    }

    private static String decode(byte[] bytes) {
        return bytes != null ? new String(bytes, StandardCharsets.UTF_8) : "";
    }

    public static final class AudioLevels {

        public final float bass;
        public final float mid;
        public final float treble;

        public AudioLevels(float bass, float mid, float treble) {
            this.bass = bass;
            this.mid = mid;
            this.treble = treble;
        }
    }

    private static native int nativeInitGl();

    private static native byte[] nativeGlStrings();

    private static native long nativeCreate(int width, int height, int meshX, int meshY, int texsizeX, int texsizeY, float fpsHint,
                                            byte[] resourcePath, byte[] textureSearchPath, byte[] cachePath, int rngSeed, boolean deterministic,
                                            boolean pinDate, float[] pinnedDate, boolean pinMouse, float mouseX, float mouseY,
                                            float maxFps, float maxLuminance, float flashLimit);

    private static native int nativeValidatePreset(byte[] path, byte[][] errorOut);

    private static native void nativeDestroy(long handle);

    private static native int nativeLoadPreset(long handle, byte[] path, float blendSeconds);

    private static native int nativeLoadPresetText(long handle, byte[] text, float blendSeconds);

    private static native void nativePushPcmFloat(long handle, float[] interleaved, int frames, int channels);

    private static native void nativePushPcmShort(long handle, short[] interleaved, int frames, int channels);

    private static native void nativeGetAudioLevels(long handle, float[] outLevels);

    private static native void nativeRender(long handle, double deltaSeconds, int targetFramebuffer);

    private static native void nativeResize(long handle, int width, int height);

    private static native void nativePause(long handle);

    private static native void nativeResume(long handle);

    private static native void nativeSetPlaybackRate(long handle, float rate);

    private static native byte[] nativeLastError(long handle);

    private static native void nativeDumpFrameState(long handle, byte[] jsonPath);

    private static native int nativeCapture(long handle, byte[] wavPath, int[] frames, byte[] outputDirectory, boolean writesFrameState, byte[][] errorOut);
}
