//
//  SynesthesiaConfiguration.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

import java.io.File;

public final class SynesthesiaConfiguration {

    public static final int CAPTURE_WIDTH = 838;
    public static final int CAPTURE_HEIGHT = 441;
    public static final int CAPTURE_RNG_SEED = 20260927;

    private static final int CAPTURE_MESH_X = 64;
    private static final int CAPTURE_MESH_Y = 48;
    private static final int CAPTURE_TEXSIZE_X = 848;
    private static final int CAPTURE_TEXSIZE_Y = 448;
    private static final float CAPTURE_FPS = 60.0f;
    private static final float PINNED_YEAR = 2000.0f;
    private static final float PINNED_MONTH = 1.0f;
    private static final float PINNED_DAY = 1.0f;
    private static final float PINNED_WEEKDAY = 6.0f;

    private final int width;
    private final int height;
    private final int meshX;
    private final int meshY;
    private final int texsizeX;
    private final int texsizeY;
    private final float fpsHint;
    private final File resourcePath;
    private final File textureSearchPath;
    private final File cachePath;
    private final int rngSeed;
    private final boolean deterministic;
    private final boolean pinDate;
    private final float[] pinnedDate;
    private final boolean pinMouse;
    private final float mouseX;
    private final float mouseY;

    private SynesthesiaConfiguration(Builder builder) {
        width = builder.width;
        height = builder.height;
        meshX = builder.meshX;
        meshY = builder.meshY;
        texsizeX = builder.texsizeX;
        texsizeY = builder.texsizeY;
        fpsHint = builder.fpsHint;
        resourcePath = builder.resourcePath;
        textureSearchPath = builder.textureSearchPath;
        cachePath = builder.cachePath;
        rngSeed = builder.rngSeed;
        deterministic = builder.deterministic;
        pinDate = builder.pinDate;
        pinnedDate = builder.pinnedDate.clone();
        pinMouse = builder.pinMouse;
        mouseX = builder.mouseX;
        mouseY = builder.mouseY;
    }

    public static Builder builder() {
        return new Builder();
    }

    public static Builder captureStandardBuilder() {
        return new Builder()
                .setSize(CAPTURE_WIDTH, CAPTURE_HEIGHT)
                .setMeshSize(CAPTURE_MESH_X, CAPTURE_MESH_Y)
                .setTextureSize(CAPTURE_TEXSIZE_X, CAPTURE_TEXSIZE_Y)
                .setFpsHint(CAPTURE_FPS)
                .setRngSeed(CAPTURE_RNG_SEED)
                .setDeterministic(true)
                .pinDate(PINNED_YEAR, PINNED_MONTH, PINNED_DAY, PINNED_WEEKDAY, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    public Builder toBuilder() {
        Builder builder = new Builder()
                .setSize(width, height)
                .setMeshSize(meshX, meshY)
                .setTextureSize(texsizeX, texsizeY)
                .setFpsHint(fpsHint)
                .setResourcePath(resourcePath)
                .setTextureSearchPath(textureSearchPath)
                .setCachePath(cachePath)
                .setRngSeed(rngSeed)
                .setDeterministic(deterministic);

        if (pinDate)
            builder.pinDate(pinnedDate[0], pinnedDate[1], pinnedDate[2], pinnedDate[3], pinnedDate[4], pinnedDate[5], pinnedDate[6], pinnedDate[7]);

        if (pinMouse)
            builder.pinMouse(mouseX, mouseY);

        return builder;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public int getMeshX() {
        return meshX;
    }

    public int getMeshY() {
        return meshY;
    }

    public int getTexsizeX() {
        return texsizeX;
    }

    public int getTexsizeY() {
        return texsizeY;
    }

    public float getFpsHint() {
        return fpsHint;
    }

    public File getResourcePath() {
        return resourcePath;
    }

    public File getTextureSearchPath() {
        return textureSearchPath;
    }

    public File getCachePath() {
        return cachePath;
    }

    public int getRngSeed() {
        return rngSeed;
    }

    public boolean isDeterministic() {
        return deterministic;
    }

    public boolean isDatePinned() {
        return pinDate;
    }

    public float[] getPinnedDate() {
        return pinnedDate.clone();
    }

    public boolean isMousePinned() {
        return pinMouse;
    }

    public float getMouseX() {
        return mouseX;
    }

    public float getMouseY() {
        return mouseY;
    }

    public static final class Builder {

        private int width;
        private int height;
        private int meshX;
        private int meshY;
        private int texsizeX;
        private int texsizeY;
        private float fpsHint;
        private File resourcePath;
        private File textureSearchPath;
        private File cachePath;
        private int rngSeed;
        private boolean deterministic;
        private boolean pinDate;
        private final float[] pinnedDate = new float[8];
        private boolean pinMouse;
        private float mouseX;
        private float mouseY;

        private Builder() {
        }

        public Builder setSize(int widthPixels, int heightPixels) {
            width = widthPixels;
            height = heightPixels;
            return this;
        }

        public Builder setMeshSize(int x, int y) {
            meshX = x;
            meshY = y;
            return this;
        }

        public Builder setTextureSize(int x, int y) {
            texsizeX = x;
            texsizeY = y;
            return this;
        }

        public Builder setFpsHint(float fps) {
            fpsHint = fps;
            return this;
        }

        public Builder setResourcePath(File path) {
            resourcePath = path;
            return this;
        }

        public Builder setTextureSearchPath(File path) {
            textureSearchPath = path;
            return this;
        }

        public Builder setCachePath(File path) {
            cachePath = path;
            return this;
        }

        public Builder setRngSeed(int seed) {
            rngSeed = seed;
            return this;
        }

        public Builder setDeterministic(boolean value) {
            deterministic = value;
            return this;
        }

        public Builder pinDate(float year, float month, float day, float weekday, float hour, float minute, float second, float milliseconds) {
            pinDate = true;
            pinnedDate[0] = year;
            pinnedDate[1] = month;
            pinnedDate[2] = day;
            pinnedDate[3] = weekday;
            pinnedDate[4] = hour;
            pinnedDate[5] = minute;
            pinnedDate[6] = second;
            pinnedDate[7] = milliseconds;
            return this;
        }

        public Builder unpinDate() {
            pinDate = false;
            return this;
        }

        public Builder pinMouse(float x, float y) {
            pinMouse = true;
            mouseX = x;
            mouseY = y;
            return this;
        }

        public Builder unpinMouse() {
            pinMouse = false;
            return this;
        }

        public SynesthesiaConfiguration build() {
            return new SynesthesiaConfiguration(this);
        }
    }
}
