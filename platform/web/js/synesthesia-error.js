//
//  synesthesia-error.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

export class SynesthesiaError extends Error {

    static SV_OK = 0;
    static SV_ERR_IO = 1;
    static SV_ERR_PARSE = 2;
    static SV_ERR_SHADER = 3;
    static SV_ERR_GL = 4;
    static SV_ERR_ARG = 5;

    constructor(statusCode, message) {
        super(SynesthesiaError.statusName(statusCode) + ": " + message);
        this.name = "SynesthesiaError";
        this.statusCode = statusCode;
    }

    get status() {
        return SynesthesiaError.statusName(this.statusCode);
    }

    static statusName(statusCode) {
        switch (statusCode) {
            case SynesthesiaError.SV_OK: return "SV_OK";
            case SynesthesiaError.SV_ERR_IO: return "SV_ERR_IO";
            case SynesthesiaError.SV_ERR_PARSE: return "SV_ERR_PARSE";
            case SynesthesiaError.SV_ERR_SHADER: return "SV_ERR_SHADER";
            case SynesthesiaError.SV_ERR_GL: return "SV_ERR_GL";
            case SynesthesiaError.SV_ERR_ARG: return "SV_ERR_ARG";
            default: return "SV_STATUS_" + statusCode;
        }
    }
}
