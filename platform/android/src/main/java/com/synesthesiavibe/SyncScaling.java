//
//  SyncScaling.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

package com.synesthesiavibe;

public final class SyncScaling {

    public static final int FILL = 0;
    public static final int FIT = 1;
    public static final int STRETCH = 2;

    private static final String[] NAMES = {"Fill", "Fit", "Stretch"};

    private SyncScaling() {
    }

    public static String name(int scaling) {
        if (scaling < FILL || scaling > STRETCH)
            return NAMES[FILL];

        return NAMES[scaling];
    }

    public static String[] names() {
        return NAMES.clone();
    }
}
