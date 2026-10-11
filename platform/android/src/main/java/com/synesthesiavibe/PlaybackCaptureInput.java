//
//  PlaybackCaptureInput.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.Manifest;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.projection.MediaProjection;

import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresPermission;

import java.util.function.Supplier;

@RequiresApi(29)
public final class PlaybackCaptureInput {

    private final AudioInput input;

    public PlaybackCaptureInput(MediaProjection projection, SynesthesiaContext context) {
        this(projection, () -> context);
    }

    public PlaybackCaptureInput(MediaProjection projection, Supplier<SynesthesiaContext> target) {
        AudioPlaybackCaptureConfiguration configuration = new AudioPlaybackCaptureConfiguration.Builder(projection)
                .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                .addMatchingUsage(AudioAttributes.USAGE_GAME)
                .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                .build();
        this.input = new AudioInput(target, new AudioInput.RecordFactory() {
            @Override
            @RequiresPermission(Manifest.permission.RECORD_AUDIO)
            public AudioRecord create(int bufferBytes) {
                return createRecord(configuration, bufferBytes);
            }
        }, "playback-capture");
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    public void start() throws SynesthesiaException {
        input.start();
    }

    public void stop() {
        input.stop();
    }

    public boolean isRunning() {
        return input.isRunning();
    }

    public long getFramesPushed() {
        return input.getFramesPushed();
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    private static AudioRecord createRecord(AudioPlaybackCaptureConfiguration configuration, int bufferBytes) {
        AudioFormat format = new AudioFormat.Builder()
                .setSampleRate(AudioInput.SAMPLE_RATE)
                .setChannelMask(AudioInput.CHANNEL_MASK)
                .setEncoding(AudioInput.ENCODING)
                .build();
        return new AudioRecord.Builder()
                .setAudioFormat(format)
                .setBufferSizeInBytes(bufferBytes)
                .setAudioPlaybackCaptureConfig(configuration)
                .build();
    }
}
