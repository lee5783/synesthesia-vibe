//
//  synesthesia-vibe-windows.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#ifndef __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_WINDOWS_H__
#define __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_WINDOWS_H__ 1

#include <synesthesia-vibe/synesthesia-vibe.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct svw_view svw_view;
typedef struct svw_group svw_group;
typedef struct svw_audio_input svw_audio_input;

typedef enum svw_adapter
{
    SVW_ADAPTER_DEFAULT = 0,
    SVW_ADAPTER_WARP = 1
} svw_adapter;

typedef enum svw_sync_scaling
{
    SVW_SYNC_SCALING_FILL = 0,
    SVW_SYNC_SCALING_FIT = 1,
    SVW_SYNC_SCALING_STRETCH = 2
} svw_sync_scaling;

typedef enum svw_audio_source
{
    SVW_AUDIO_LOOPBACK = 0,
    SVW_AUDIO_MICROPHONE = 1
} svw_audio_source;

typedef struct svw_view_config
{
    void* hwnd;
    float render_scale;
    int   adapter;
    bool  start_paused;
} svw_view_config;

typedef struct svw_group_config
{
    int   adapter;
    bool  start_paused;
} svw_group_config;

/* Every svw_* call may run on any thread. Calls that return a result wait for the view's render thread; called from inside an svw_invoke_fn or svw_snapshot_fn they run at once. Callbacks run on the view's render thread with its GL context current, never on the caller's thread. */
typedef void (*svw_invoke_fn)(sv_context* ctx, uint32_t target_fbo, void* user);
typedef void (*svw_snapshot_fn)(const uint8_t* rgba_top_down, int width, int height, void* user);
typedef void (*svw_pcm_fn)(const float* interleaved, size_t frames, int channels, void* user);

/* engine may be NULL. Its width and height are ignored: the backing size follows the window's client area in physical pixels times render_scale. The strings are copied. */
sv_status   svw_view_create(const svw_view_config* config, const sv_config* engine, svw_view** out_view);
void        svw_view_destroy(svw_view* view);

/* With a NULL view, the last error of a call on this thread that had no view, such as a failed svw_view_create. */
const char* svw_last_error(svw_view* view);

sv_status   svw_view_load_preset(svw_view* view, const char* path, float blend_seconds);
sv_status   svw_view_load_preset_memory(svw_view* view, const char* text, size_t len, float blend_seconds);

sv_status   svw_view_pause(svw_view* view);
sv_status   svw_view_resume(svw_view* view);

sv_status   svw_view_set_playback_rate(svw_view* view, float rate);
/* fps <= 0 means the refresh rate of the window's monitor; the rate never exceeds it. */
sv_status   svw_view_set_preferred_fps(svw_view* view, float fps);
/* Clamped to [0.25, 1]. */
sv_status   svw_view_set_render_scale(svw_view* view, float scale);

/* fn receives the next rendered frame, top-down RGBA8 with a stride of width * 4, borrowed for the duration of the call. */
sv_status   svw_view_snapshot(svw_view* view, svw_snapshot_fn fn, void* user);
/* Writes the next rendered frame as a PNG to a UTF-8 path; from inside an svw_invoke_fn, the frame last drawn to target_fbo. */
sv_status   svw_view_snapshot_png(svw_view* view, const char* path);

sv_status   svw_view_invoke(svw_view* view, svw_invoke_fn fn, void* user);

/* NULL leaves. While grouped the view's own thread is parked: preset loads and svw_view_invoke go to the group, and the view's own setters are stored until it leaves. Not callable from a callback. */
sv_status   svw_view_set_sync_group(svw_view* view, svw_group* group);
sv_status   svw_view_set_sync_scaling(svw_view* view, int scaling);

sv_status   svw_group_create(const svw_group_config* config, const sv_config* engine, svw_group** out_group);
void        svw_group_destroy(svw_group* group);
const char* svw_group_last_error(svw_group* group);

sv_status   svw_group_load_preset(svw_group* group, const char* path, float blend_seconds);
sv_status   svw_group_load_preset_memory(svw_group* group, const char* text, size_t len, float blend_seconds);

sv_status   svw_group_pause(svw_group* group);
sv_status   svw_group_resume(svw_group* group);

sv_status   svw_group_set_playback_rate(svw_group* group, float rate);
sv_status   svw_group_set_preferred_fps(svw_group* group, float fps);
sv_status   svw_group_set_render_scale(svw_group* group, float scale);
/* 0, 0 returns to the per-axis maximum of the members' surface sizes times the render scale. */
sv_status   svw_group_set_fixed_render_size(svw_group* group, int width, int height);
sv_status   svw_group_get_render_size(svw_group* group, int* out_width, int* out_height);
sv_status   svw_group_get_member_count(svw_group* group, int* out_count);

sv_status   svw_group_invoke(svw_group* group, svw_invoke_fn fn, void* user);

/* The engine and svw_pcm_fn receive 44.1 kHz stereo float32 interleaved PCM after gain; svw_pcm_fn runs on the capture thread. */
sv_status   svw_audio_input_create(int source, svw_audio_input** out_input);
void        svw_audio_input_destroy(svw_audio_input* input);
const char* svw_audio_input_last_error(svw_audio_input* input);
const char* svw_audio_input_device_name(svw_audio_input* input);

sv_status   svw_audio_input_start(svw_audio_input* input);
sv_status   svw_audio_input_stop(svw_audio_input* input);

sv_status   svw_audio_input_set_gain(svw_audio_input* input, float gain);
sv_status   svw_audio_input_set_pcm_callback(svw_audio_input* input, svw_pcm_fn fn, void* user);
/* sv_get_audio_levels of the handle that rendered last; 0 when none is running. */
sv_status   svw_audio_input_get_levels(svw_audio_input* input, float* out_bass, float* out_mid, float* out_treb);

#ifdef __cplusplus
}
#endif

#endif
