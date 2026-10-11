//
//  shader_text_gen.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PARSER_SHADER_TEXT_GEN_H__
#define __BEATDROP_PARSER_SHADER_TEXT_GEN_H__ 1

void GenWarpPShaderText(char *szShaderText, const char *szDefaultWarpPShaderText, float decay, bool bWrap);
void GenCompPShaderText(char *szShaderText, const char *szDefaultCompPShaderText, float brightness, float ve_alpha, float ve_zoom, int ve_orient, float hue_shader, bool bBrighten, bool bDarken, bool bSolarize, bool bInvert);

#endif
