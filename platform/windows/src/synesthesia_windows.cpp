//
//  synesthesia_windows.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#include <synesthesia-vibe/synesthesia-vibe-windows.h>

#include "audio_input.h"
#include "audio_router.h"
#include "host_support.h"
#include "render_view.h"
#include "sync_group.h"

#include <string>

namespace
{
    thread_local std::string t_lastError;
    thread_local std::string t_returnedError;

    sv_status FailWithoutView(sv_status status, const std::string& message)
    {
        t_lastError = message;
        return status;
    }

    sv_status NullView(const char* function)
    {
        return FailWithoutView(SV_ERR_ARG, std::string(function) + ": view is null");
    }

    sv_status NullGroup(const char* function)
    {
        return FailWithoutView(SV_ERR_ARG, std::string(function) + ": group is null");
    }

    sv_status NullInput(const char* function)
    {
        return FailWithoutView(SV_ERR_ARG, std::string(function) + ": input is null");
    }
}

extern "C" sv_status svw_view_create(const svw_view_config* config, const sv_config* engine, svw_view** out_view)
{
    if (out_view == nullptr)
        return FailWithoutView(SV_ERR_ARG, "svw_view_create: out_view is null");

    *out_view = nullptr;
    if (config == nullptr)
        return FailWithoutView(SV_ERR_ARG, "svw_view_create: config is null");

    if (svw::OnHostRenderThread())
        return FailWithoutView(SV_ERR_ARG, "svw_view_create: called from a render thread");

    std::string error;
    const sv_status status = svw_view::Create(*config, engine, *out_view, error);
    if (status != SV_OK)
        return FailWithoutView(status, "svw_view_create: " + error);

    return SV_OK;
}

extern "C" void svw_view_destroy(svw_view* view)
{
    if (view == nullptr)
        return;

    if (view->OnRenderThread() || svw::OnHostRenderThread())
    {
        view->Fail(SV_ERR_ARG, "svw_view_destroy: called from a render thread");
        return;
    }

    view->SetSyncGroup(nullptr);
    delete view;
}

extern "C" const char* svw_last_error(svw_view* view)
{
    t_returnedError = view != nullptr ? view->LastError() : t_lastError;
    return t_returnedError.c_str();
}

extern "C" sv_status svw_view_load_preset(svw_view* view, const char* path, float blend_seconds)
{
    if (view == nullptr)
        return NullView("svw_view_load_preset");

    return view->LoadPreset(path, blend_seconds);
}

extern "C" sv_status svw_view_load_preset_memory(svw_view* view, const char* text, size_t len, float blend_seconds)
{
    if (view == nullptr)
        return NullView("svw_view_load_preset_memory");

    return view->LoadPresetMemory(text, len, blend_seconds);
}

extern "C" sv_status svw_view_pause(svw_view* view)
{
    if (view == nullptr)
        return NullView("svw_view_pause");

    return view->Pause();
}

extern "C" sv_status svw_view_resume(svw_view* view)
{
    if (view == nullptr)
        return NullView("svw_view_resume");

    return view->Resume();
}

extern "C" sv_status svw_view_set_playback_rate(svw_view* view, float rate)
{
    if (view == nullptr)
        return NullView("svw_view_set_playback_rate");

    return view->SetPlaybackRate(rate);
}

extern "C" sv_status svw_view_set_preferred_fps(svw_view* view, float fps)
{
    if (view == nullptr)
        return NullView("svw_view_set_preferred_fps");

    return view->SetPreferredFps(fps);
}

extern "C" sv_status svw_view_set_render_scale(svw_view* view, float scale)
{
    if (view == nullptr)
        return NullView("svw_view_set_render_scale");

    return view->SetRenderScale(scale);
}

extern "C" sv_status svw_view_snapshot(svw_view* view, svw_snapshot_fn fn, void* user)
{
    if (view == nullptr)
        return NullView("svw_view_snapshot");

    return view->Snapshot(fn, user);
}

extern "C" sv_status svw_view_snapshot_png(svw_view* view, const char* path)
{
    if (view == nullptr)
        return NullView("svw_view_snapshot_png");

    return view->SnapshotPng(path);
}

extern "C" sv_status svw_view_invoke(svw_view* view, svw_invoke_fn fn, void* user)
{
    if (view == nullptr)
        return NullView("svw_view_invoke");

    return view->Invoke(fn, user);
}

extern "C" sv_status svw_view_set_sync_group(svw_view* view, svw_group* group)
{
    if (view == nullptr)
        return NullView("svw_view_set_sync_group");

    return view->SetSyncGroup(group);
}

extern "C" sv_status svw_view_set_sync_scaling(svw_view* view, int scaling)
{
    if (view == nullptr)
        return NullView("svw_view_set_sync_scaling");

    return view->SetSyncScaling(scaling);
}

extern "C" sv_status svw_group_create(const svw_group_config* config, const sv_config* engine, svw_group** out_group)
{
    if (out_group == nullptr)
        return FailWithoutView(SV_ERR_ARG, "svw_group_create: out_group is null");

    *out_group = nullptr;
    if (config == nullptr)
        return FailWithoutView(SV_ERR_ARG, "svw_group_create: config is null");

    if (svw::OnHostRenderThread())
        return FailWithoutView(SV_ERR_ARG, "svw_group_create: called from a render thread");

    std::string error;
    const sv_status status = svw_group::Create(*config, engine, *out_group, error);
    if (status != SV_OK)
        return FailWithoutView(status, "svw_group_create: " + error);

    return SV_OK;
}

extern "C" void svw_group_destroy(svw_group* group)
{
    if (group == nullptr)
        return;

    if (svw::OnHostRenderThread())
    {
        group->Fail(SV_ERR_ARG, "svw_group_destroy: called from a render thread");
        return;
    }

    group->Close();
}

extern "C" const char* svw_group_last_error(svw_group* group)
{
    t_returnedError = group != nullptr ? group->LastError() : t_lastError;
    return t_returnedError.c_str();
}

extern "C" sv_status svw_group_load_preset(svw_group* group, const char* path, float blend_seconds)
{
    if (group == nullptr)
        return NullGroup("svw_group_load_preset");

    return group->LoadPreset(path, blend_seconds);
}

extern "C" sv_status svw_group_load_preset_memory(svw_group* group, const char* text, size_t len, float blend_seconds)
{
    if (group == nullptr)
        return NullGroup("svw_group_load_preset_memory");

    return group->LoadPresetMemory(text, len, blend_seconds);
}

extern "C" sv_status svw_group_pause(svw_group* group)
{
    if (group == nullptr)
        return NullGroup("svw_group_pause");

    return group->Pause();
}

extern "C" sv_status svw_group_resume(svw_group* group)
{
    if (group == nullptr)
        return NullGroup("svw_group_resume");

    return group->Resume();
}

extern "C" sv_status svw_group_set_playback_rate(svw_group* group, float rate)
{
    if (group == nullptr)
        return NullGroup("svw_group_set_playback_rate");

    return group->SetPlaybackRate(rate);
}

extern "C" sv_status svw_group_set_preferred_fps(svw_group* group, float fps)
{
    if (group == nullptr)
        return NullGroup("svw_group_set_preferred_fps");

    return group->SetPreferredFps(fps);
}

extern "C" sv_status svw_group_set_render_scale(svw_group* group, float scale)
{
    if (group == nullptr)
        return NullGroup("svw_group_set_render_scale");

    return group->SetRenderScale(scale);
}

extern "C" sv_status svw_group_set_fixed_render_size(svw_group* group, int width, int height)
{
    if (group == nullptr)
        return NullGroup("svw_group_set_fixed_render_size");

    return group->SetFixedRenderSize(width, height);
}

extern "C" sv_status svw_group_get_render_size(svw_group* group, int* out_width, int* out_height)
{
    if (group == nullptr)
        return NullGroup("svw_group_get_render_size");

    if (out_width == nullptr || out_height == nullptr)
        return group->Fail(SV_ERR_ARG, "svw_group_get_render_size: an out pointer is null");

    return group->GetRenderSize(*out_width, *out_height);
}

extern "C" sv_status svw_group_get_member_count(svw_group* group, int* out_count)
{
    if (group == nullptr)
        return NullGroup("svw_group_get_member_count");

    if (out_count == nullptr)
        return group->Fail(SV_ERR_ARG, "svw_group_get_member_count: out_count is null");

    return group->GetMemberCount(*out_count);
}

extern "C" sv_status svw_group_invoke(svw_group* group, svw_invoke_fn fn, void* user)
{
    if (group == nullptr)
        return NullGroup("svw_group_invoke");

    return group->Invoke(fn, user);
}

extern "C" sv_status svw_audio_input_create(int source, svw_audio_input** out_input)
{
    if (out_input == nullptr)
        return FailWithoutView(SV_ERR_ARG, "svw_audio_input_create: out_input is null");

    *out_input = nullptr;
    if (source != SVW_AUDIO_LOOPBACK && source != SVW_AUDIO_MICROPHONE)
        return FailWithoutView(SV_ERR_ARG, "svw_audio_input_create: source must be SVW_AUDIO_LOOPBACK or SVW_AUDIO_MICROPHONE");

    *out_input = new svw_audio_input(source);
    return SV_OK;
}

extern "C" void svw_audio_input_destroy(svw_audio_input* input)
{
    if (input == nullptr)
        return;

    if (input->OnCaptureThread())
    {
        input->Fail(SV_ERR_ARG, "svw_audio_input_destroy: called from the svw_pcm_fn callback");
        return;
    }

    delete input;
}

extern "C" const char* svw_audio_input_last_error(svw_audio_input* input)
{
    t_returnedError = input != nullptr ? input->LastError() : t_lastError;
    return t_returnedError.c_str();
}

extern "C" const char* svw_audio_input_device_name(svw_audio_input* input)
{
    t_returnedError = input != nullptr ? input->DeviceName() : std::string();
    return t_returnedError.c_str();
}

extern "C" sv_status svw_audio_input_start(svw_audio_input* input)
{
    if (input == nullptr)
        return NullInput("svw_audio_input_start");

    return input->Start();
}

extern "C" sv_status svw_audio_input_stop(svw_audio_input* input)
{
    if (input == nullptr)
        return NullInput("svw_audio_input_stop");

    return input->Stop();
}

extern "C" sv_status svw_audio_input_set_gain(svw_audio_input* input, float gain)
{
    if (input == nullptr)
        return NullInput("svw_audio_input_set_gain");

    return input->SetGain(gain);
}

extern "C" sv_status svw_audio_input_set_pcm_callback(svw_audio_input* input, svw_pcm_fn fn, void* user)
{
    if (input == nullptr)
        return NullInput("svw_audio_input_set_pcm_callback");

    return input->SetPcmCallback(fn, user);
}

extern "C" sv_status svw_audio_input_get_levels(svw_audio_input* input, float* out_bass, float* out_mid, float* out_treb)
{
    if (input == nullptr)
        return NullInput("svw_audio_input_get_levels");

    float bass = 0.0f;
    float mid = 0.0f;
    float treb = 0.0f;
    svw::LatestAudioLevels(bass, mid, treb);
    if (out_bass != nullptr)
        *out_bass = bass;
    if (out_mid != nullptr)
        *out_mid = mid;
    if (out_treb != nullptr)
        *out_treb = treb;
    return SV_OK;
}
