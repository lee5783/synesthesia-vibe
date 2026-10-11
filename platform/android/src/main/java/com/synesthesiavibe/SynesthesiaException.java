//
//  SynesthesiaException.java
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe;

public final class SynesthesiaException extends Exception {

    public static final int SV_OK = 0;
    public static final int SV_ERR_IO = 1;
    public static final int SV_ERR_PARSE = 2;
    public static final int SV_ERR_SHADER = 3;
    public static final int SV_ERR_GL = 4;
    public static final int SV_ERR_ARG = 5;

    private final int statusCode;

    public SynesthesiaException(int statusCode, String message) {
        super(statusName(statusCode) + ": " + message);
        this.statusCode = statusCode;
    }

    public int getStatusCode() {
        return statusCode;
    }

    public String getStatus() {
        return statusName(statusCode);
    }

    public static String statusName(int statusCode) {
        switch (statusCode) {
            case SV_OK: return "SV_OK";
            case SV_ERR_IO: return "SV_ERR_IO";
            case SV_ERR_PARSE: return "SV_ERR_PARSE";
            case SV_ERR_SHADER: return "SV_ERR_SHADER";
            case SV_ERR_GL: return "SV_ERR_GL";
            case SV_ERR_ARG: return "SV_ERR_ARG";
            default: return "SV_STATUS_" + statusCode;
        }
    }
}
