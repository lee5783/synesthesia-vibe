//
//  TransitionConfiguration.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe;

public final class TransitionConfiguration {

    public static final float DEFAULT_SOFT_CUT_SECONDS = 2.7f;
    public static final float DEFAULT_PRESET_DURATION = 16.0f;
    public static final float DEFAULT_PRESET_DURATION_RAND = 10.0f;
    public static final float DEFAULT_HARD_CUT_LOUDNESS = 2.5f;
    public static final float DEFAULT_HARD_CUT_HALFLIFE = 60.0f;

    private final float softCutSeconds;
    private final float presetDuration;
    private final float presetDurationRand;
    private final boolean hardCutsEnabled;
    private final float hardCutLoudness;
    private final float hardCutHalflife;

    private TransitionConfiguration(Builder builder) {
        softCutSeconds = builder.softCutSeconds;
        presetDuration = builder.presetDuration;
        presetDurationRand = builder.presetDurationRand;
        hardCutsEnabled = builder.hardCutsEnabled;
        hardCutLoudness = builder.hardCutLoudness;
        hardCutHalflife = builder.hardCutHalflife;
    }

    public static Builder builder() {
        return new Builder();
    }

    public Builder toBuilder() {
        return new Builder()
                .setSoftCutSeconds(softCutSeconds)
                .setPresetDuration(presetDuration)
                .setPresetDurationRand(presetDurationRand)
                .setHardCutsEnabled(hardCutsEnabled)
                .setHardCutLoudness(hardCutLoudness)
                .setHardCutHalflife(hardCutHalflife);
    }

    public float getSoftCutSeconds() {
        return softCutSeconds;
    }

    public float getPresetDuration() {
        return presetDuration;
    }

    public float getPresetDurationRand() {
        return presetDurationRand;
    }

    public boolean isHardCutsEnabled() {
        return hardCutsEnabled;
    }

    public float getHardCutLoudness() {
        return hardCutLoudness;
    }

    public float getHardCutHalflife() {
        return hardCutHalflife;
    }

    public static final class Builder {

        private float softCutSeconds = DEFAULT_SOFT_CUT_SECONDS;
        private float presetDuration = DEFAULT_PRESET_DURATION;
        private float presetDurationRand = DEFAULT_PRESET_DURATION_RAND;
        private boolean hardCutsEnabled;
        private float hardCutLoudness = DEFAULT_HARD_CUT_LOUDNESS;
        private float hardCutHalflife = DEFAULT_HARD_CUT_HALFLIFE;

        private Builder() {
        }

        public Builder setSoftCutSeconds(float seconds) {
            softCutSeconds = seconds;
            return this;
        }

        public Builder setPresetDuration(float seconds) {
            presetDuration = seconds;
            return this;
        }

        public Builder setPresetDurationRand(float seconds) {
            presetDurationRand = seconds;
            return this;
        }

        public Builder setHardCutsEnabled(boolean enabled) {
            hardCutsEnabled = enabled;
            return this;
        }

        public Builder setHardCutLoudness(float loudness) {
            hardCutLoudness = loudness;
            return this;
        }

        public Builder setHardCutHalflife(float halflife) {
            hardCutHalflife = halflife;
            return this;
        }

        public TransitionConfiguration build() {
            return new TransitionConfiguration(this);
        }
    }
}
