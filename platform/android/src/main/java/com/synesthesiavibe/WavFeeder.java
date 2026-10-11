//
//  WavFeeder.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.util.Log;

import java.io.File;
import java.nio.charset.StandardCharsets;

public final class WavFeeder implements AutoCloseable {

    public static final int FRAMES_PER_RENDER_FRAME = 735;

    private static final String LOG_TAG = "SynesthesiaVibe";
    private static final int INFO_FIELDS = 3;

    static {
        System.loadLibrary("synesthesiavibe-jni");
    }

    private final File file;
    private final int channels;
    private final int sampleRate;
    private final long frameCount;
    private long handle;
    private short[] buffer = new short[0];
    private double frameRemainder;
    private long framesFed;

    public WavFeeder(File wav) throws SynesthesiaException {
        long opened = nativeOpen(wav.getPath().getBytes(StandardCharsets.UTF_8));

        if (opened == 0)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "could not load 16-bit PCM WAV " + wav);

        long[] info = new long[INFO_FIELDS];
        nativeInfo(opened, info);
        this.file = wav;
        this.handle = opened;
        this.channels = (int) info[0];
        this.sampleRate = (int) info[1];
        this.frameCount = info[2];
        Log.i(LOG_TAG, "WavFeeder open " + wav.getName() + ": " + frameCount + " frames, " + channels + " ch, " + sampleRate + " Hz");
    }

    public File getFile() {
        return file;
    }

    public int getChannels() {
        return channels;
    }

    public int getSampleRate() {
        return sampleRate;
    }

    public long getFrameCount() {
        return frameCount;
    }

    public synchronized long getFramesFed() {
        return framesFed;
    }

    public int feedNextRenderFrame(SynesthesiaContext context) {
        return feedFrames(context, FRAMES_PER_RENDER_FRAME);
    }

    public synchronized int feed(SynesthesiaContext context, double deltaSeconds) {
        if (deltaSeconds <= 0.0 || sampleRate <= 0)
            return 0;

        double frames = frameRemainder + deltaSeconds * sampleRate;
        double wholeFrames = Math.floor(frames);
        frameRemainder = frames - wholeFrames;
        return feedFrames(context, (int) Math.min(wholeFrames, frameCount));
    }

    public synchronized int feedFrames(SynesthesiaContext context, int frames) {
        if (handle == 0)
            throw new IllegalStateException("WavFeeder is closed");

        if (frames <= 0)
            return 0;

        int samples = frames * channels;

        if (buffer.length < samples)
            buffer = new short[samples];

        int read = nativeRead(handle, frames, buffer);

        if (read <= 0)
            return 0;

        context.pushPcm(buffer, read, channels);
        framesFed += read;
        return read;
    }

    public synchronized void reset() {
        frameRemainder = 0.0;
    }

    @Override
    public synchronized void close() {
        if (handle == 0)
            return;

        nativeClose(handle);
        handle = 0;
        Log.i(LOG_TAG, "WavFeeder close " + file.getName() + ": fed " + framesFed + " frames");
    }

    private static native long nativeOpen(byte[] path);

    private static native void nativeInfo(long handle, long[] outInfo);

    private static native int nativeRead(long handle, int frames, short[] outInterleaved);

    private static native void nativeClose(long handle);
}
