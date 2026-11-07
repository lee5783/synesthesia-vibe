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
typedef struct sv_prepared_preset sv_prepared_preset;

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

    float max_fps;

    float max_luminance;
    float flash_limit;
} sv_config;

typedef void* (*sv_gl_get_proc_fn)(const char* name);

sv_status   sv_init_gl(sv_gl_get_proc_fn get_proc);

sv_context* sv_create(const sv_config* config);
void        sv_destroy(sv_context* ctx);

sv_status   sv_load_preset(sv_context* ctx, const char* path, float blend_seconds);
sv_status   sv_load_preset_memory(sv_context* ctx, const char* text, size_t len, float blend_seconds);

/* sv_validate_preset* need no context or GL and may run on any thread, concurrently with rendering, loads and other validates or prepares, once an sv_create in the process has returned. */
sv_status   sv_validate_preset(const char* path, char* error, size_t error_len);
sv_status   sv_validate_preset_memory(const char* text, size_t len, char* error, size_t error_len);

/* sv_prepare_preset* do the CPU half of a load on any thread: read and parse the preset, compile its code without running it, and translate its shaders. They read only the context's resource, texture and cache paths, never its live state or GL, so they may run while the context renders; the host must not destroy the context during a prepare. On success *out_prepared is a new handle and error is ""; on failure *out_prepared is NULL, error holds the reason and the codes match sv_load_preset* (a shader that does not translate is not an error: the commit falls back to fixed-function and reports it in sv_last_error). */
sv_status   sv_prepare_preset(sv_context* ctx, const char* path, sv_prepared_preset** out_prepared, char* error, size_t error_len);
sv_status   sv_prepare_preset_memory(sv_context* ctx, const char* text, size_t len, sv_prepared_preset** out_prepared, char* error, size_t error_len);

/* sv_commit_preset finishes the load on the GL thread, like sv_load_preset with the prepared result: init code, random preset values, the blend and GL programs run here, at commit time. A handle commits once, to the context that prepared it; a null, committed or foreign handle gives SV_ERR_ARG, and SV_ERR_GL (the GL pipeline exists but is not ready) is returned before anything changes, leaving the handle and the context as they were, where sv_load_preset* would load and then report SV_ERR_GL; a context without a GL pipeline commits the CPU state only. The handle still needs sv_prepared_preset_free after a commit. */
sv_status   sv_commit_preset(sv_context* ctx, sv_prepared_preset* prepared, float blend_seconds);

/* sv_prepared_preset_free releases a handle, committed or not, on any thread, also after its context was destroyed. NULL is ignored. */
void        sv_prepared_preset_free(sv_prepared_preset* prepared);

void        sv_push_pcm_float(sv_context* ctx, const float* interleaved, size_t frames, int channels);
void        sv_push_pcm_s16(sv_context* ctx, const int16_t* interleaved, size_t frames, int channels);
void        sv_get_audio_levels(sv_context* ctx, float* out_bass, float* out_mid, float* out_treb);

/* The shader path the current preset took, as recorded when its load or commit bound the GL programs. *_ps_version is the pixel-shader version the preset asked for: 0 = none (fixed-function by format), 2 = ps_2_0, 3 = ps_2_x, 4 = ps_3_0. *_shader is 1 when that stage runs a shader, 0 when it runs fixed-function or there is no GL. */
typedef struct sv_preset_info
{
    int  preset_version;       /* MILKDROP_PRESET_VERSION of the current preset; 0 before any load */
    int  warp_ps_version;
    int  comp_ps_version;
    int  warp_shader;
    int  comp_shader;
    char fallback_reason[256]; /* NUL-terminated, truncated; why a stage that asked for a shader runs fixed-function instead, naming the stage; "no GL pipeline" or "GL pipeline not ready" without a usable GL pipeline; may contain newlines; "" when there is no fallback */
} sv_preset_info;

/* Fills *out and returns SV_OK, or SV_ERR_ARG when ctx or out is NULL. May run on any thread, concurrently with rendering and loads: it reads a copy the engine refreshes under a lock at each load or commit, never the live preset. A load that fails before it replaces the preset leaves the previous preset's values. */
sv_status   sv_get_preset_info(sv_context* ctx, sv_preset_info* out);

/* sv_init_gl, sv_create, sv_load_preset*, sv_commit_preset, sv_resize, sv_render_frame, sv_set_debug_callback, sv_set_debug_interval and sv_destroy must run on the thread whose GL context is current; width/height are backing pixels. */
void        sv_render_frame(sv_context* ctx, double dt_seconds, uint32_t target_fbo);
void        sv_resize(sv_context* ctx, int width, int height);

/* sv_pause stops the simulation until sv_resume; while paused, sv_render_frame only runs the debug report and, on an interactive context, redraws the last frame to the target. Resume continues the preset time without a jump or reset. */
void        sv_pause(sv_context* ctx);
void        sv_resume(sv_context* ctx);

void        sv_set_playback_rate(sv_context* ctx, float rate);

const char* sv_last_error(sv_context* ctx);
void        sv_dump_frame_state(sv_context* ctx, const char* json_path);

/* Periodic diagnostics report, filled by the engine. It and its strings are borrowed: valid only for the duration of the callback, so copy what you keep. */
typedef struct sv_debug_info
{
    double   time;                /* the preset-visible time variable, in simulation seconds */
    uint32_t frame;               /* the preset frame counter, cumulative across preset loads; sv_dump_frame_state reports frame - 1 */
    float    display_fps;         /* sv_render_frame calls per wall second, measured over the report window */
    float    sim_steps_per_frame; /* simulation steps per sv_render_frame call over the report window; 1 on deterministic contexts, 0 while paused */
    float    playback_rate;       /* the effective rate: 1 on deterministic contexts */
    int      preset_version;      /* MILKDROP_PRESET_VERSION of the current (incoming, while blending) preset; 0 before any load */
    int      warp_shader;         /* 1 = warp pixel shader active, 0 = fixed-function or no GL */
    int      comp_shader;         /* 1 = composite pixel shader active, 0 = fixed-function or no GL */
    float    bass, mid, treb, bass_att, mid_att, treb_att;
    int      texsize_x, texsize_y, width, height;
    int      feedback_format;     /* 0 = RGBA8 */
    int      gl_errors;           /* GL errors drained since the previous report; process-wide; 0 with no GL */
    int      limiter_active;      /* 1 = luminance limiter configured on; 0 = off or no GL */
    const char* preset_name;      /* the preset file name without its extension; "" before any load and for sv_load_preset_memory */
    const char* last_error;       /* same as sv_last_error */
    int      warp_ps_version;     /* the current preset's warp pixel-shader version: 0 = fixed-function by format, 2 = ps_2_0, 3 = ps_2_x, 4 = ps_3_0; 0 before any load */
    int      comp_ps_version;     /* same for the composite stage */
    const char* fallback_reason;  /* why a stage that asked for a shader runs fixed-function instead, naming the stage; "no GL pipeline" or "GL pipeline not ready" without a usable GL pipeline; may contain newlines; "" when there is no fallback */
} sv_debug_info;

/* Runs synchronously inside sv_render_frame, on the thread that calls it, once the report interval has elapsed in wall-clock time; it also runs while paused if the host keeps calling sv_render_frame.
   It may call sv_set_debug_callback, sv_set_debug_interval, sv_get_audio_levels, sv_get_preset_info and sv_last_error. It must not call sv_destroy, sv_load_preset*, sv_pause, sv_resume, sv_resize, sv_render_frame or sv_dump_frame_state. */
typedef void (*sv_debug_fn)(const sv_debug_info* info, void* user);

/* Off by default. NULL disables. Takes effect on the next sv_render_frame and restarts the report window. */
void        sv_set_debug_callback(sv_context* ctx, sv_debug_fn fn, void* user);
/* Default 1.0 seconds; <= 0 disables; a non-finite value means 1.0. Restarts the report window. An interval shorter than a frame reports once per sv_render_frame while frames are under 1 second apart.
   sv_resume and a gap between sv_render_frame calls longer than both the interval and 1 second also restart the window, without a report. */
void        sv_set_debug_interval(sv_context* ctx, float seconds);

#ifdef __cplusplus
}
#endif

#endif