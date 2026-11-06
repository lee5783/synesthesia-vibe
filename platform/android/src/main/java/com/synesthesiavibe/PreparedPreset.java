//
//  PreparedPreset.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe;

public final class PreparedPreset implements AutoCloseable {

    private long handle;

    PreparedPreset(long handle) {
        this.handle = handle;
    }

    public synchronized boolean isClosed() {
        return handle == 0;
    }

    synchronized int commitTo(long contextHandle, float blendSeconds) {
        if (handle == 0)
            throw new IllegalStateException("PreparedPreset is closed");
        return SynesthesiaContext.nativeCommitPreset(contextHandle, handle, blendSeconds);
    }

    @Override
    public synchronized void close() {
        if (handle != 0) {
            SynesthesiaContext.nativeFreePrepared(handle);
            handle = 0;
        }
    }
}
