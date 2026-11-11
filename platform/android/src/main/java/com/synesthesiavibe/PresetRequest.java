//
//  PresetRequest.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe;

public final class PresetRequest {

    public static final int REASON_TIMER = 0;
    public static final int REASON_HARD_CUT = 1;

    private final int reason;
    private final float suggestedBlendSeconds;

    PresetRequest(int reason, float suggestedBlendSeconds) {
        this.reason = reason;
        this.suggestedBlendSeconds = suggestedBlendSeconds;
    }

    public int getReason() {
        return reason;
    }

    public boolean isHardCut() {
        return reason == REASON_HARD_CUT;
    }

    public float getSuggestedBlendSeconds() {
        return suggestedBlendSeconds;
    }
}
