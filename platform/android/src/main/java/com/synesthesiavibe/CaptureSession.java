//
//  CaptureSession.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import android.os.Build;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public final class CaptureSession {

    public static final String GL_STRINGS_FILE_NAME = "gl-strings.txt";

    private final SynesthesiaConfiguration configuration;
    private final File preset;
    private final File wav;
    private final int[] frames;
    private final File outputDirectory;
    private boolean writesFrameState = true;
    private List<File> writtenFiles = Collections.emptyList();

    public CaptureSession(SynesthesiaConfiguration configuration, File preset, File wav, int[] frames, File outputDirectory) {
        SynesthesiaConfiguration.Builder standard = SynesthesiaConfiguration.captureStandardBuilder()
                .setResourcePath(configuration.getResourcePath())
                .setTextureSearchPath(configuration.getTextureSearchPath())
                .setCachePath(configuration.getCachePath());

        if (configuration.isDatePinned()) {
            float[] date = configuration.getPinnedDate();
            standard.pinDate(date[0], date[1], date[2], date[3], date[4], date[5], date[6], date[7]);
        }

        if (configuration.isMousePinned())
            standard.pinMouse(configuration.getMouseX(), configuration.getMouseY());

        this.configuration = standard.build();
        this.preset = preset;
        this.wav = wav;
        this.frames = Arrays.stream(frames).filter(frame -> frame >= 0).sorted().distinct().toArray();
        this.outputDirectory = outputDirectory;
    }

    public SynesthesiaConfiguration getConfiguration() {
        return configuration;
    }

    public boolean writesFrameState() {
        return writesFrameState;
    }

    public void setWritesFrameState(boolean value) {
        writesFrameState = value;
    }

    public List<File> getWrittenFiles() {
        return writtenFiles;
    }

    public File fileForFrame(int frame, String extension) {
        return new File(outputDirectory, String.format(Locale.ROOT, "frame_%04d.%s", frame, extension));
    }

    public void run() throws SynesthesiaException {
        if (frames.length == 0)
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_ARG, "capture frame list is empty");

        if (!outputDirectory.isDirectory() && !outputDirectory.mkdirs())
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "could not create " + outputDirectory);

        SynesthesiaContext.initGl();

        try (SynesthesiaContext context = SynesthesiaContext.create(configuration)) {
            context.loadPreset(preset, 0.0f);
            writeGlStrings();
            context.captureFrames(wav, frames, outputDirectory, writesFrameState);
        }

        List<File> written = new ArrayList<>();

        for (int frame : frames) {
            written.add(fileForFrame(frame, "png"));

            if (writesFrameState)
                written.add(fileForFrame(frame, "json"));
        }

        writtenFiles = Collections.unmodifiableList(written);
    }

    private void writeGlStrings() throws SynesthesiaException {
        String text = SynesthesiaContext.glStrings()
                + "ABI=" + Build.SUPPORTED_ABIS[0] + "\n"
                + "SDK_INT=" + Build.VERSION.SDK_INT + "\n";

        try (FileOutputStream stream = new FileOutputStream(new File(outputDirectory, GL_STRINGS_FILE_NAME))) {
            stream.write(text.getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) {
            throw new SynesthesiaException(SynesthesiaException.SV_ERR_IO, "could not write " + GL_STRINGS_FILE_NAME + ": " + error.getMessage());
        }
    }
}
