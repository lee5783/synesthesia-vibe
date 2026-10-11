//
//  analysis_settings.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-28.
//

#ifndef __BEATDROP_CONFIG_ANALYSIS_SETTINGS_H__
#define __BEATDROP_CONFIG_ANALYSIS_SETTINGS_H__ 1

struct AudioAnalysisSettings
{
    int   bassStart;
    int   bassEnd;
    int   midStart;
    int   midEnd;
    int   trebStart;
    int   trebEnd;
    float fftShaderScaling;
    float fftShaderNoiseFloor;
    float audioSensitivity;
};

struct ParserCapabilities
{
    bool isAMD;
    int  maxPSVersion;
};

#endif
