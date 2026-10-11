//
//  preset_shader_header.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "preset_shader_header.h"

namespace render_gl
{

namespace
{

const char* const kPresetShaderHeader = R"HLSL(
#define  M_PI   3.14159265359
#define  M_PI_2 6.28318530718
#define  M_INV_PI_2  0.159154943091895

float4   rand_frame;
float4   rand_preset;
float4   _c0;
float4   _c1, _c2, _c3, _c4;
float4   _c5;
float4   _c6;
float4   _c7;
float4   _c8;
float4   _c9;
float4   _c10;
float4   _c11;
float4   _c12;
float4   _c13;
float4   _c14;
float4   _c15;
float4   _c16;
float4   _qa;
float4   _qb;
float4   _qc;
float4   _qd;
float4   _qe;
float4   _qf;
float4   _qg;
float4   _qh;

float4x3 rot_s1;
float4x3 rot_s2;
float4x3 rot_s3;
float4x3 rot_s4;

float4x3 rot_d1;
float4x3 rot_d2;
float4x3 rot_d3;
float4x3 rot_d4;
float4x3 rot_f1;
float4x3 rot_f2;
float4x3 rot_f3;
float4x3 rot_f4;
float4x3 rot_vf1;
float4x3 rot_vf2;
float4x3 rot_vf3;
float4x3 rot_vf4;
float4x3 rot_uf1;
float4x3 rot_uf2;
float4x3 rot_uf3;
float4x3 rot_uf4;

float4x3 rot_rand1;
float4x3 rot_rand2;
float4x3 rot_rand3;
float4x3 rot_rand4;

#define time     _c2.x
#define fps      _c2.y
#define frame    _c2.z
#define progress _c2.w
#define bass _c3.x
#define mid  _c3.y
#define treb _c3.z
#define vol  _c3.w
#define bass_att _c4.x
#define mid_att  _c4.y
#define treb_att _c4.z
#define vol_att  _c4.w
#define q1 _qa.x
#define q2 _qa.y
#define q3 _qa.z
#define q4 _qa.w
#define q5 _qb.x
#define q6 _qb.y
#define q7 _qb.z
#define q8 _qb.w
#define q9 _qc.x
#define q10 _qc.y
#define q11 _qc.z
#define q12 _qc.w
#define q13 _qd.x
#define q14 _qd.y
#define q15 _qd.z
#define q16 _qd.w
#define q17 _qe.x
#define q18 _qe.y
#define q19 _qe.z
#define q20 _qe.w
#define q21 _qf.x
#define q22 _qf.y
#define q23 _qf.z
#define q24 _qf.w
#define q25 _qg.x
#define q26 _qg.y
#define q27 _qg.z
#define q28 _qg.w
#define q29 _qh.x
#define q30 _qh.y
#define q31 _qh.z
#define q32 _qh.w

#define aspect   _c0
#define aspect_sd _c1
#define texsize  _c7
#define roam_cos _c8
#define roam_sin _c9
#define slow_roam_cos _c10
#define slow_roam_sin _c11
#define mip_x   _c12.x
#define mip_y   _c12.y
#define mip_xy  _c12.xy
#define mip_avg _c12.z
#define blur1_min _c6.z
#define blur1_max _c6.w
#define blur2_min _c13.x
#define blur2_max _c13.y
#define blur3_min _c13.z
#define blur3_max _c13.w
#define mouse _c14
#define mouse_x _c14.x
#define mouse_y _c14.y
#define mouse_pos _c14.xy
#define mouse_clicked _c14.z
#define mouse_released _c14.w
#define sysTime _c15
#define sysDate _c16
#define sysTimeHour _c15.x
#define sysTimeMinute _c15.y
#define sysTimeSecond _c15.z
#define sysTimeTotalSeconds _c15.w
#define sysDateYear _c16.x
#define sysDateMonth _c16.y
#define sysDateDay _c16.z
#define sysDateWeekday _c16.w
#define GetMain(uv) (tex2D(sampler_main,uv).xyz)
#define GetPixel(uv) (tex2D(sampler_main,uv).xyz)
#define GetBlur1(uv) (tex2D(sampler_blur1,uv).xyz*_c5.x + _c5.y)
#define GetBlur2(uv) (tex2D(sampler_blur2,uv).xyz*_c5.z + _c5.w)
#define GetBlur3(uv) (tex2D(sampler_blur3,uv).xyz*_c6.x + _c6.y)

#define SAMPLER1D sampler1D
#define SAMPLER2D sampler2D
#define SAMPLER3D sampler3D
#define texture1D tex1D
#define texture2D tex2D
#define texture3D tex3D

#define lum(x) (dot(x,float3(0.32,0.49,0.29)))
#define trunc(x) ((x) < 0 ? ceil((x)) : floor((x)))
#define tex1d tex1D
#define tex2d tex2D
#define tex3d tex3D
#define texcube texCUBE
#define texCube texCUBE

sampler2D sampler_main    ;
sampler2D sampler_fc_main ;
sampler2D sampler_pc_main ;
sampler2D sampler_fw_main ;
sampler2D sampler_pw_main ;
#define sampler_FC_main sampler_fc_main
#define sampler_PC_main sampler_pc_main
#define sampler_FW_main sampler_fw_main
#define sampler_PW_main sampler_pw_main

sampler2D sampler_noise_lq;
sampler2D sampler_noise_lq_lite;
sampler2D sampler_noise_mq;
sampler2D sampler_noise_hq;
sampler3D sampler_noisevol_lq;
sampler3D sampler_noisevol_hq;
float4 texsize_noise_lq;
float4 texsize_noise_lq_lite;
float4 texsize_noise_mq;
float4 texsize_noise_hq;
float4 texsize_noisevol_lq;
float4 texsize_noisevol_hq;

sampler2D sampler_blur1;
sampler2D sampler_blur2;
sampler2D sampler_blur3;

sampler2D sampler_fft;
#define HAS_FFT_PEAK 1

float get_fft(float pos) {
    return sqrt(tex2D(sampler_fft, float2(saturate(pos), .25)).x);
}

float get_fft_hz(float freq) {
    return get_fft(freq / 22050.0);
}

float get_fft_peak(float pos) {
    return sqrt(tex2D(sampler_fft, float2(saturate(pos), 0.75)).x);
}

float get_fft_peak_hz(float freq) {
    return get_fft_peak(freq / 22050.0);
}

sampler2D sampler_wave;

float get_wave(float pos) {
    float left = tex2D(sampler_wave, float2(saturate(pos), 0.25)).x;
    float right = tex2D(sampler_wave, float2(saturate(pos), 0.75)).x;
    return (left + right) * 0.5;
}

float get_wave_left(float pos) {
    return tex2D(sampler_wave, float2(saturate(pos), 0.25)).x;
}

float get_wave_right(float pos) {
    return tex2D(sampler_wave, float2(saturate(pos), 0.75)).x;
}
)HLSL";

}

const char* PresetShaderHeaderText()
{
    return kPresetShaderHeader + 1;
}

}
