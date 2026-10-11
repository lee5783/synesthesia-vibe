//
//  AudioInput.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.Manifest;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Process;
import android.util.Log;

import androidx.annotation.RequiresPermission;

import java.util.function.Supplier;

public final class AudioInput {

    public static final int SAMPLE_RATE = 44100;
    public static final int CHANNELS = 2;
    public static final int CHANNEL_MASK = AudioFormat.CHANNEL_IN_STEREO;
    public static final int ENCODING = AudioFormat.ENCODING_PCM_16BIT;

    private static final String LOG_TAG = "SynesthesiaVibe";
    private static final int READ_FRAMES = 512;
    private static final int BYTES_PER_FRAME = CHANNELS * 2;
    private static final int BUFFER_MULTIPLIER = 4;

    interface RecordFactory {
        @RequiresPermission(Manifest.permission.RECORD_AUDIO)
        AudioRecord create(int bufferBytes);
    }

    private final Supplier<SynesthesiaContext> target;
    private final RecordFactory factory;
    private final String label;
    private AudioRecord record;
    private Thread reader;
    private volatile boolean running;
    private volatile long framesPushed;

    public AudioInput(SynesthesiaContext context) {
        this(() -> context);
    }

    public AudioInput(Supplier<SynesthesiaContext> target) {
        this(target, MediaRecorder.AudioSource.VOICE_RECOGNITION);
    }

    public AudioInput(Supplier<SynesthesiaContext> target, int audioSource) {
        this(target, new RecordFactory() {
            @Override
            @RequiresPermission(Manifest.permission.RECORD_AUDIO)
            public AudioRecord create(int bufferBytes) {
                return new AudioRecord(audioSource, SAMPLE_RATE, CHANNEL_MASK, ENCODING, bufferBytes);
            }
        }, "source=" + audioSource);
    }

    AudioInput(Supplier<SynesthesiaContext> target, RecordFactory factory, String label) {
        this.target = target;
        this.factory = factory;
        this.label = label;
    }

    public static int preferredAudioSource(Context context) {
        AudioManager manager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        String unprocessed = manager != null ? manager.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED) : null;
        return Boolean.parseBoolean(unprocessed) ? MediaRecorder.AudioSource.UNPROCESSED : MediaRecorder.AudioSource.VOICE_RECOGNITION;
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    public synchronized void start() throws SynesthesiaException {
        if (running)
            return;

        int minimumBytes = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_MASK, ENCODING);

        if (minimumBytes <= 0)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "AudioRecord does not support " + SAMPLE_RATE + " Hz stereo 16-bit");

        int bufferBytes = Math.max(minimumBytes, READ_FRAMES * BYTES_PER_FRAME) * BUFFER_MULTIPLIER;
        AudioRecord created;

        try {
            created = factory.create(bufferBytes);
        } catch (RuntimeException error) {
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "AudioRecord could not be created (" + label + "): " + error.getMessage());
        }

        if (created.getState() != AudioRecord.STATE_INITIALIZED) {
            created.release();
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "AudioRecord could not be initialized (" + label + "); is RECORD_AUDIO granted?");
        }

        created.startRecording();

        if (created.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
            created.release();
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "AudioRecord did not start recording (" + label + ")");
        }

        record = created;
        framesPushed = 0;
        running = true;
        reader = new Thread(() -> readLoop(created), "SynesthesiaAudioInput");
        reader.start();
        Log.i(LOG_TAG, "AudioRecord start (" + label + "): " + created.getSampleRate() + " Hz, " + created.getChannelCount() + " ch, buffer " + bufferBytes + " bytes");
    }

    public synchronized void stop() {
        if (record == null)
            return;

        running = false;
        record.stop();

        try {
            reader.join();
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
        }

        record.release();
        record = null;
        reader = null;
        Log.i(LOG_TAG, "AudioRecord stop (" + label + "): pushed " + framesPushed + " frames");
    }

    public boolean isRunning() {
        return running;
    }

    public long getFramesPushed() {
        return framesPushed;
    }

    private void readLoop(AudioRecord source) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
        short[] buffer = new short[READ_FRAMES * CHANNELS];

        while (running) {
            int read = source.read(buffer, 0, buffer.length);

            if (read < 0) {
                if (running)
                    Log.w(LOG_TAG, "AudioRecord read failed (" + label + "): " + read);
                break;
            }

            int frames = read / CHANNELS;

            if (frames > 0)
                push(buffer, frames);
        }

        running = false;
    }

    private void push(short[] buffer, int frames) {
        SynesthesiaContext context = target.get();

        if (context == null)
            return;

        try {
            context.pushPcm(buffer, frames, CHANNELS);
            framesPushed += frames;
        } catch (IllegalStateException closed) {
            return;
        }
    }
}
