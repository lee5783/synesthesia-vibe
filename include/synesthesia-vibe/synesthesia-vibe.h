//
//  synesthesia-vibe.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-06.
//

#ifndef __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_H__
#define __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_H__ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_context sv_context;

typedef enum sv_status
{
    SV_OK = 0,
    SV_ERR_IO = 1,
    SV_ERR_PARSE = 2,
    SV_ERR_SHADER = 3,
    SV_ERR_GL = 4,
    SV_ERR_ARG = 5
} sv_status;

typedef struct sv_config
{
    int   width, height;
    int   mesh_x, mesh_y;
    int   texsize_x, texsize_y;
    float fps_hint;

    const char* resource_path;
    const char* texture_search_path;

    uint32_t rng_seed;

    bool deterministic;

    bool pin_date;
    float year, month, day, weekday, hour, minute, second, milliseconds;

    bool pin_mouse;
    float mouse_x, mouse_y;

    /* Writable directory for sv_load_preset_memory's temporary file; NULL falls back to $TEMP, then $TMPDIR, then ".". */
    const char* cache_path;
} sv_config;

typedef void* (*sv_gl_get_proc_fn)(const char* name);

sv_status   sv_init_gl(sv_gl_get_proc_fn get_proc);

sv_context* sv_create(const sv_config* config);
void        sv_destroy(sv_context* ctx);

sv_status   sv_load_preset(sv_context* ctx, const char* path, float blend_seconds);
sv_status   sv_load_preset_memory(sv_context* ctx, const char* text, size_t len, float blend_seconds);

void        sv_push_pcm_float(sv_context* ctx, const float* interleaved, size_t frames, int channels);
void        sv_push_pcm_s16(sv_context* ctx, const int16_t* interleaved, size_t frames, int channels);
void        sv_get_audio_levels(sv_context* ctx, float* out_bass, float* out_mid, float* out_treb);

/* sv_init_gl, sv_create, sv_load_preset*, sv_resize, sv_render_frame and sv_destroy must run on the thread whose GL context is current; width/height are backing pixels. */
void        sv_render_frame(sv_context* ctx, double dt_seconds, uint32_t target_fbo);
void        sv_resize(sv_context* ctx, int width, int height);

/* sv_pause makes sv_render_frame a no-op until sv_resume; resume continues the preset time without a jump or reset. */
void        sv_pause(sv_context* ctx);
void        sv_resume(sv_context* ctx);

const char* sv_last_error(sv_context* ctx);
void        sv_dump_frame_state(sv_context* ctx, const char* json_path);

#ifdef __cplusplus
}
#endif

#endif