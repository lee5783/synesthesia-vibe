//
//  sampler_resolver.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-06.
//

#include "sampler_resolver.h"

#include <cstdio>
#include <cstring>

namespace render_core
{

static bool IsNumericChar(char x)
{
    return x >= '0' && x <= '9';
}

void ResolveSamplerName(const char* szConstantName, int nBlurTexCount,
                        IRandomTextureSource* pRandomTextures, RandomTextureSlots& randSlots,
                        SamplerResolution& out)
{
    // remove "sampler_" prefix to create root file name.  could still have "FW_" prefix or something like that.
    char szRootName[kSamplerRootNameMax];
    if (!strncmp(szConstantName, "sampler_", 8))
        snprintf(szRootName, sizeof(szRootName), "%s", &szConstantName[8]);
    else
        snprintf(szRootName, sizeof(szRootName), "%s", szConstantName);

    // also peel off "XY_" prefix, if it's there, to specify filtering & wrap mode.
    bool bBilinear = true;
    bool bWrap     = true;
    bool bWrapFilterSpecified = false;
    if (strlen(szRootName) > 3 && szRootName[2]=='_')
    {
        char temp[3];
        temp[0] = szRootName[0];
        temp[1] = szRootName[1];
        temp[2] = 0;
        // convert to uppercase
        if (temp[0] >= 'a' && temp[0] <= 'z')
            temp[0] -= 'a' - 'A';
        if (temp[1] >= 'a' && temp[1] <= 'z')
            temp[1] -= 'a' - 'A';

        if      (!strcmp(temp, "FW")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = true; }
        else if (!strcmp(temp, "FC")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = false; }
        else if (!strcmp(temp, "PW")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = true; }
        else if (!strcmp(temp, "PC")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = false; }
        // also allow reverses:
        else if (!strcmp(temp, "WF")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = true; }
        else if (!strcmp(temp, "CF")) { bWrapFilterSpecified = true; bBilinear = true;  bWrap = false; }
        else if (!strcmp(temp, "WP")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = true; }
        else if (!strcmp(temp, "CP")) { bWrapFilterSpecified = true; bBilinear = false; bWrap = false; }

        // peel off the prefix
        int i = 0;
        while (szRootName[i+3])
        {
            szRootName[i] = szRootName[i+3];
            i++;
        }
        szRootName[i] = 0;
    }

    TexCode texCode = TexCode::Disk;

    // if <szFileName> is "main", map it to the VS...
    if (!strcmp("main", szRootName))
    {
        texCode = TexCode::Vs;
    }
    else if (nBlurTexCount >= 2 && !strcmp("blur1", szRootName))
    {
        texCode = TexCode::Blur1;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (nBlurTexCount >= 4 && !strcmp("blur2", szRootName))
    {
        texCode = TexCode::Blur2;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (nBlurTexCount >= 6 && !strcmp("blur3", szRootName))
    {
        texCode = TexCode::Blur3;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (nBlurTexCount >= 8 && !strcmp("blur4", szRootName))
    {
        texCode = TexCode::Blur4;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (nBlurTexCount >= 10 && !strcmp("blur5", szRootName))
    {
        texCode = TexCode::Blur5;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (nBlurTexCount >= 12 && !strcmp("blur6", szRootName))
    {
        texCode = TexCode::Blur6;
        if (!bWrapFilterSpecified) { // when sampling blur textures, default is CLAMP
            bWrap = false;
            bBilinear = true;
        }
    }
    else if (!strcmp("fft", szRootName)) {
        texCode = TexCode::Fft;
        if (!bWrapFilterSpecified) {
            bWrap = false;   // clamp
            bBilinear = true; // linear interpolation between bins
        }
    }
    else if (!strcmp("wave", szRootName)) {
        texCode = TexCode::Wave;
        if (!bWrapFilterSpecified) {
            bWrap = false;   // clamp
            bBilinear = true; // linear interpolation between samples
        }
    }
    else
    {
        texCode = TexCode::Disk;

        // check for request for random texture.
        if (!strncmp("rand", szRootName, 4) &&
            IsNumericChar(szRootName[4]) &&
            IsNumericChar(szRootName[5]) &&
            (szRootName[6]==0 || szRootName[6]=='_') )
        {
            int rand_slot = -1;

            // peel off filename prefix ("rand13_smalltiled", for example)
            char prefix[kSamplerRootNameMax];
            if (szRootName[6]=='_')
                snprintf(prefix, sizeof(prefix), "%s", &szRootName[7]);
            else
                prefix[0] = 0;
            szRootName[6] = 0;

            sscanf(&szRootName[4], "%d", &rand_slot);
            if (rand_slot >= 0 && rand_slot <= kMaxRandomTextureSlot - 1)      // otherwise, not a special filename - ignore it
            {
                bool bPicked = false;
                if (pRandomTextures)
                    bPicked = pRandomTextures->PickRandomTexture(prefix, szRootName, kSamplerRootNameMax);

                if (!bPicked)
                {
                    if (prefix[0])
                        snprintf(szRootName, sizeof(szRootName), "[rand%02d] %s*", rand_slot, prefix);
                    else
                        snprintf(szRootName, sizeof(szRootName), "[rand%02d] *", rand_slot);
                }
                else
                {
                    //chop off extension
                    char *p = strrchr(szRootName, '.');
                    if (p)
                        *p = 0;
                }

                snprintf(randSlots.names[rand_slot], kSamplerRootNameMax, "%s", szRootName); // we'll need to remember this for texsize_ params!
            }
        }
    }

    snprintf(out.rootName, kSamplerRootNameMax, "%s", szRootName);
    out.texCode = texCode;
    out.bWrap = bWrap;
    out.bBilinear = bBilinear;
}

Float4ConstantResolution ResolveFloat4Constant(const char* szConstantName, bool bMatrixColumns, bool bVector,
                                               const RandomTextureSlots& randSlots)
{
    Float4ConstantResolution res;
    res.kind = Float4ConstantKind::None;
    res.index = -1;
    res.rootName[0] = 0;

    if (bMatrixColumns)
    {
        static const char* const kRotNames[kConstVectorHandleCount] = {
            "rot_s1", "rot_s2", "rot_s3", "rot_s4",
            "rot_d1", "rot_d2", "rot_d3", "rot_d4",
            "rot_f1", "rot_f2", "rot_f3", "rot_f4",
            "rot_vf1", "rot_vf2", "rot_vf3", "rot_vf4",
            "rot_uf1", "rot_uf2", "rot_uf3", "rot_uf4",
            "rot_rand1", "rot_rand2", "rot_rand3", "rot_rand4"
        };
        for (int i=0; i<kConstVectorHandleCount; i++)
        {
            if (!strcmp(szConstantName, kRotNames[i]))
            {
                res.kind = Float4ConstantKind::RotMatrix;
                res.index = i;
                return res;
            }
        }
    }
    else if (bVector)
    {
        if      (!strcmp(szConstantName, "rand_frame"))  { res.kind = Float4ConstantKind::RandFrame;  return res; }
        else if (!strcmp(szConstantName, "rand_preset")) { res.kind = Float4ConstantKind::RandPreset; return res; }
        else if (!strncmp(szConstantName, "texsize_", 8))
        {
            // remove "texsize_" prefix to find root file name.
            char szRootName[kSamplerRootNameMax];
            snprintf(szRootName, sizeof(szRootName), "%s", &szConstantName[8]);

            // check for request for random texture.
            // it should be a previously-seen random index - just fetch/reuse the name.
            if (!strncmp("rand", szRootName, 4) &&
                IsNumericChar(szRootName[4]) &&
                IsNumericChar(szRootName[5]) &&
                (szRootName[6]==0 || szRootName[6]=='_') )
            {
                int rand_slot = -1;

                // ditch filename prefix ("rand13_smalltiled", for example)
                // and just go by the slot
                if (szRootName[6]=='_')
                    szRootName[6] = 0;

                sscanf(&szRootName[4], "%d", &rand_slot);
                if (rand_slot >= 0 && rand_slot <= 15)      // otherwise, not a special filename - ignore it
                    if (randSlots.names[rand_slot][0])
                        snprintf(szRootName, sizeof(szRootName), "%s", randSlots.names[rand_slot]);
            }

            res.kind = Float4ConstantKind::TexSize;
            snprintf(res.rootName, kSamplerRootNameMax, "%s", szRootName);
            return res;
        }
        else if (szConstantName[0] == '_' && szConstantName[1] == 'c')
        {
            int z;
            if (sscanf(&szConstantName[2], "%d", &z)==1)
                if (z >= 0 && z < kConstVectorHandleCount)
                {
                    res.kind = Float4ConstantKind::ConstVector;
                    res.index = z;
                }
        }
        else if (szConstantName[0] == '_' && szConstantName[1] == 'q')
        {
            int z = szConstantName[2] - 'a';
            if (z >= 0 && z < kShaderConstQVectorCount)
            {
                res.kind = Float4ConstantKind::QConst;
                res.index = z;
            }
        }
    }

    return res;
}

}