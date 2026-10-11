//
//  shader_text.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-06.
//

#include "shader_text.h"

#include "md_defines.h"

#include <cstdio>
#include <cstring>

namespace render_core
{

void StripComments(char* str)
{
    if (!str || !str[0] || !str[1])
        return;

    char c0 = str[0];
    char c1 = str[1];
    char* dest = str;
    char* p = &str[1];
    bool bIgnoreTilEndOfLine = false;
    bool bIgnoreTilCloseComment = false; //this one takes precedence
    int nCharsToSkip = 0;
    while (1)
    {
        // handle '//' comments
        if (!bIgnoreTilCloseComment && c0=='/' && c1=='/')
            bIgnoreTilEndOfLine = true;
        if (bIgnoreTilEndOfLine && (c0==10 || c0==13))
        {
            bIgnoreTilEndOfLine = false;
            nCharsToSkip = 0;
        }

        // handle /* */ comments
        if (!bIgnoreTilEndOfLine && c0=='/' && c1=='*')
            bIgnoreTilCloseComment = true;
        if (bIgnoreTilCloseComment && c0=='*' && c1=='/')
        {
            bIgnoreTilCloseComment = false;
            nCharsToSkip = 2;
        }

        if (!bIgnoreTilEndOfLine && !bIgnoreTilCloseComment)
        {
            if (nCharsToSkip > 0)
                nCharsToSkip--;
            else
                *dest++ = c0;
        }

        if (c1==0)
            break;

        p++;
        c0 = c1;
        c1 = *p;
    }

    *dest++ = 0;
}

bool ReadResourceToString(const char* szUtf8ResourceRoot, const char* szUtf8RelativeName, char* szDestText, int nMaxBytes, bool bConvertLFsToSpecialChar)
{
    char szFile[4096];
    snprintf(szFile, sizeof(szFile), "%s%s", szUtf8ResourceRoot, szUtf8RelativeName);

    // read in all chars.  Replace char combos:  { 13;  13+10;  10 } with LINEFEED_CONTROL_CHAR, if bConvertLFsToSpecialChar is true.
    FILE* f = fopen(szFile, "rb");
    if (!f)
    {
        return false;
    }
    int len = 0;
    int x;
    char prev_ch = 0;
    while ( (x = fgetc(f)) >= 0 && len < nMaxBytes-4 )
    {
        char orig_ch = (char)x;
        char ch = orig_ch;
        bool bSkipChar = false;
        if (bConvertLFsToSpecialChar)
        {
            if (ch==10)
            {
                if (prev_ch==13)
                    bSkipChar = true;
                else
                    ch = LINEFEED_CONTROL_CHAR;
            }
            else if (ch==13)
                ch = LINEFEED_CONTROL_CHAR;
        }

        if (!bSkipChar)
            szDestText[len++] = ch;
        prev_ch = orig_ch;
    }
    szDestText[len] = 0;
    szDestText[len++] = ' ';   // make sure there is some whitespace after
    fclose(f);
    return true;
}

const char* ShaderStageName(ShaderStage stage)
{
    switch(stage)
    {
    case ShaderStage::Warp:      return "warp";
    case ShaderStage::Composite: return "composite";
    case ShaderStage::Blur:      return "blur";
    case ShaderStage::Other:     return "(other)";
    default:                     return "(unknown)";
    }
}

ShaderTextResult AssembleShaderText(const ShaderTextInput& input, char* szShaderText, ShaderTextOutput& out)
{
    const char szWarpDefines[] = "#define rad _rad_ang.x\n"
                                 "#define ang _rad_ang.y\n"
                                 "#define uv _uv.xy\n"
                                 "#define uv_orig _uv.zw\n";
    const char szCompDefines[] = "#define rad _rad_ang.x\n"
                                 "#define ang _rad_ang.y\n"
                                 "#define uv _uv.xy\n"
                                 "#define uv_orig _uv.xy\n" //[sic]
                                 "#define hue_shader _vDiffuse.xyz\n";
    const char szWarpParams[] = "float4 _vDiffuse : COLOR, float4 _uv : TEXCOORD0, float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    const char szCompParams[] = "float4 _vDiffuse : COLOR, float2 _uv : TEXCOORD0, float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    const char szFirstLine[]  = "    float3 ret = 0;";
    const char szLastLine[]   = "    _return_value = float4(ret.xyz, _vDiffuse.w);";

    char temp[128000];
    int writePos = 0;

    // paste the universal #include
    memcpy(&szShaderText[writePos], input.includeText, input.includeTextLen);  // first, paste in the contents of 'inputs.fx' before the actual shader text.  Has 13's and 10's.
    writePos += input.includeTextLen;

    // paste in any custom #defines for this shader type
    if (input.stage == ShaderStage::Warp && input.bPixelProfile)
    {
        strcpy(&szShaderText[writePos], szWarpDefines);
        writePos += (int)strlen(szWarpDefines);
    }
    else if (input.stage == ShaderStage::Composite && input.bPixelProfile)
    {
        strcpy(&szShaderText[writePos], szCompDefines);
        writePos += (int)strlen(szCompDefines);
    }

    // paste in the shader itself - converting LCC's to 13+10's.
    // avoid strcpy b/c it might not handle the linefeed stuff...?
    int shaderStartPos = writePos;
    {
        const char *s = input.shaderSource;
        char *d = &szShaderText[writePos];
        while (*s)
        {
            if (*s == LINEFEED_CONTROL_CHAR)
            {
                *d++ = 13; writePos++;
                *d++ = 10; writePos++;
            }
            else
            {
                *d++ = *s; writePos++;
            }
            s++;
        }
        *d = 0; writePos++;
    }

    // strip out all comments - but cheat a little - start at the shader test.
    // (the include file was already stripped of comments)
    StripComments(&szShaderText[shaderStartPos]);

    //note: only do this stuff if type is WARP or COMP shader... not for blur, etc!
    //FIXME - hints on the inputs / output / samplers etc.
    //   can go in the menu header, NOT the preset!  =)
    //then update presets
    //  -> be sure to update the presets on disk AND THE DEFAULT SHADERS (for loading MD1 presets)
    //FIXME - then update auth. guide w/new examples,
    //   and a list of the invisible inputs (and one output) to each shader!
    //   warp: float2 uv, float2 uv_orig, rad, ang
    //   comp: float2 uv, rad, ang, float3 hue_shader
    // test all this string code in Debug mode - make sure nothing bad is happening

    /*
    1. paste warp or comp #defines
    2. search for "void" + whitespace + szFn + [whitespace] + '('
    3. insert params
    4. search for [whitespace] + ')'.
    5. search for final '}' (strrchr)
    6. back up one char, insert the Last Line, and add '}' and that's it.
    */
    if ((input.stage == ShaderStage::Warp || input.stage == ShaderStage::Composite) && input.bPixelProfile)
    {
        char* p = &szShaderText[shaderStartPos];

        // seek to 'shader_body' and replace it with spaces
        while (*p && strncmp(p, "shader_body", 11))
            p++;
        if (p)
        {
            for (int i=0; i<11; i++)
                *p++ = ' ';
        }

        if (p)
        {
            // insert "void PS(...params...)\n"
            strcpy(temp, p);
            const char *params = (input.stage == ShaderStage::Warp) ? szWarpParams : szCompParams;
            sprintf(p, "void %s( %s )\n", input.entryPoint, params);
            p += strlen(p);
            strcpy(p, temp);

            // find the starting curly brace
            p = strchr(p, '{');
            if (p)
            {
                // skip over it
                p++;
                // then insert "float3 ret = 0;"
                strcpy(temp, p);
                sprintf(p, "%s\n", szFirstLine);
                p += strlen(p);
                strcpy(p, temp);

                // find the ending curly brace
                p = strrchr(p, '}');
                // add the last line - "    _return_value = float4(ret.xyz, _vDiffuse.w);"
                if (p)
                    sprintf(p, " %s\n}\n", szLastLine);
            }
        }

        if (!p)
        {
            return ShaderTextResult::ShaderBodyNotFound;
        }
    }

    out.shaderStartPos = shaderStartPos;
    out.textLength = (int)strlen(szShaderText);
    return ShaderTextResult::Ok;
}

}