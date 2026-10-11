//
//  shader_text_gen.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#include "shader_text_gen.h"
#include "md_defines.h"
#include "parser_win32.h"
#include <cstdio>
#include <cstring>

void GenWarpPShaderText(char *szShaderText, const char *szDefaultWarpPShaderText, float decay, bool bWrap)
{
    // find the pixel shader body and replace it with custom code.

    ParserLstrcpyA(szShaderText, szDefaultWarpPShaderText);
    char LF = LINEFEED_CONTROL_CHAR;
    char *p = strrchr( szShaderText, '{' );
    if (!p)
        return;
    p++;
    p += sprintf(p, "%c", 1);

    p += sprintf(p, "    // sample previous frame%c", LF);
	// SPOUT
	// Avoid freeze
	p += sprintf(p, "    ret = tex2D( sampler%ls_main, uv ).xyz;%c", bWrap ? L"" : L"_fc", LF);
	// p += sprintf(p, "    ret = tex2D( sampler%s_main, uv ).xyz;%c", bWrap ? L"" : L"_fc", LF);
    p += sprintf(p, "    %c", LF);
    p += sprintf(p, "    // darken (decay) over time%c", LF);
    p += sprintf(p, "    ret *= %.2f; //or try: ret -= 0.004;%c", decay, LF);
    //p += sprintf(p, "    %c", LF);
    //p += sprintf(p, "    ret.w = vDiffuse.w; // pass alpha along - req'd for preset blending%c", LF);
    p += sprintf(p, "}%c", LF);
}

void GenCompPShaderText(char *szShaderText, const char *szDefaultCompPShaderText, float brightness, float ve_alpha, float ve_zoom, int ve_orient, float hue_shader, bool bBrighten, bool bDarken, bool bSolarize, bool bInvert)
{
    // find the pixel shader body and replace it with custom code.

    ParserLstrcpyA(szShaderText, szDefaultCompPShaderText);
    char LF = LINEFEED_CONTROL_CHAR;
    char *p = strrchr( szShaderText, '{' );
    if (!p)
        return;
    p++;
    p += sprintf(p, "%c", 1);

    if (ve_alpha > 0.001f)
    {
        int orient_x = (ve_orient %  2) ? -1 : 1;
        int orient_y = (ve_orient >= 2) ? -1 : 1;
        p += sprintf(p, "    float2 uv_echo = (uv - 0.5)*%.3f*float2(%d,%d) + 0.5;%c", 1.0f/ve_zoom, orient_x, orient_y, LF);
        p += sprintf(p, "    ret = lerp( tex2D(sampler_main, uv).xyz, %c", LF);
        p += sprintf(p, "                tex2D(sampler_main, uv_echo).xyz, %c", LF);
        p += sprintf(p, "                %.2f %c", ve_alpha, LF);
        p += sprintf(p, "              ); //video echo%c", LF);
        p += sprintf(p, "    ret *= %.2f; //gamma%c", brightness, LF);
    }
    else
    {
        p += sprintf(p, "    ret = tex2D(sampler_main, uv).xyz;%c", LF);
        p += sprintf(p, "    ret *= %.2f; //gamma%c", brightness, LF);
    }
    if (hue_shader >= 1.0f)
        p += sprintf(p, "    ret *= hue_shader; //old hue shader effect%c", LF);
    else if (hue_shader > 0.001f)
        p += sprintf(p, "    ret *= %.2f + %.2f*hue_shader; //old hue shader effect%c", 1-hue_shader, hue_shader, LF);

    if (bBrighten)
        p += sprintf(p, "    ret = 1 - (1-ret) * (1-ret); //brighten%c", LF);
    if (bDarken)
        p += sprintf(p, "    ret *= ret; //darken%c", LF);
    if (bSolarize)
        p += sprintf(p, "    ret = ret*(1-ret)*2; //solarize%c", LF);
    if (bInvert)
        p += sprintf(p, "    ret = 1 - ret; //invert%c", LF);
    //p += sprintf(p, "    ret.w = vDiffuse.w; // pass alpha along - req'd for preset blending%c", LF);
    p += sprintf(p, "}%c", LF);
}
