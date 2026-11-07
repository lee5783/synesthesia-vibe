//
//  BlendPattern.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe;

public final class BlendPattern {

    public static final int RANDOM = -1;
    public static final int COUNT = 19;

    private static final String[] NAMES = {
        "Fade", "Directional Wipe", "Plasma", "Radial", "Clock", "Spiral", "Diamond", "Nuclear Clock",
        "Square", "Checkerboard Wipe", "Curtain", "Bubble", "Kaleidoscope", "Moebius Strip", "Star",
        "Disco Floor", "Fire", "Whirlpool Drain", "Julia Fractal"
    };

    private BlendPattern() {
    }

    public static String name(int pattern) {
        if (pattern < 0 || pattern >= COUNT)
            return "Random";

        return NAMES[pattern];
    }

    public static String[] names() {
        String[] all = new String[COUNT + 1];
        all[0] = name(RANDOM);
        System.arraycopy(NAMES, 0, all, 1, COUNT);
        return all;
    }
}
