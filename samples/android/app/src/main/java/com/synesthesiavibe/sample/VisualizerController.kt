//
//  VisualizerController.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.Manifest
import android.media.projection.MediaProjection
import android.opengl.GLSurfaceView
import android.os.Build
import android.util.Log
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.synesthesiavibe.AudioInput
import com.synesthesiavibe.PlaybackCaptureInput
import com.synesthesiavibe.SynesthesiaConfiguration
import com.synesthesiavibe.SynesthesiaContext
import com.synesthesiavibe.SynesthesiaException
import com.synesthesiavibe.SynesthesiaRenderer
import com.synesthesiavibe.WavFeeder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

enum class AudioSource { Wav, Microphone, SystemAudio }

class VisualizerController(
    private val library: SampleLibrary,
    initialPreset: Preset,
    widthPixels: Int,
    heightPixels: Int,
    private val preserveEglContext: Boolean,
) {

    var preset by mutableStateOf(initialPreset)
        private set
    var audioSource by mutableStateOf(AudioSource.Wav)
        private set
    var errorMessage by mutableStateOf<String?>(null)
        private set

    private val renderer = SynesthesiaRenderer(
        SynesthesiaConfiguration.builder()
            .setSize(widthPixels, heightPixels)
            .setResourcePath(library.resourceRoot)
            .build(),
        object : SynesthesiaRenderer.Listener {
            override fun onContextCreated(context: SynesthesiaContext) {
                Log.i(TAG, "context created")
                load(context, loadedPreset, 0f)
            }

            override fun onContextError(error: SynesthesiaException) {
                Log.e(TAG, "context error", error)
                errorMessage = error.message
            }
        },
    )
    private val wavFeeder: WavFeeder? = library.defaultWav?.let(::WavFeeder)
    private var surface: GLSurfaceView? = null
    private var liveInput: LiveInput? = null

    @Volatile
    private var loadedPreset: Preset = initialPreset

    @Volatile
    private var feedsWav = true

    @Volatile
    private var lastFeedNanos = 0L

    private val glRenderer = object : GLSurfaceView.Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = renderer.onSurfaceCreated(gl, config)

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = renderer.onSurfaceChanged(gl, width, height)

        override fun onDrawFrame(gl: GL10?) {
            feedWav()
            renderer.onDrawFrame(gl)
        }
    }

    fun attach(view: GLSurfaceView) {
        SynesthesiaRenderer.configure(view)
        view.preserveEGLContextOnPause = preserveEglContext
        view.setRenderer(glRenderer)
        view.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        surface = view
        Log.i(TAG, "surface attached preserveEGLContextOnPause=$preserveEglContext")
    }

    fun detach() {
        stopLiveInput()
        val view = surface ?: return
        surface = null
        view.queueEvent {
            renderer.release()
            wavFeeder?.close()
        }
        view.onPause()
    }

    fun selectPreset(next: Preset) {
        preset = next
        loadedPreset = next
        surface?.queueEvent { renderer.context?.let { load(it, next, BLEND_SECONDS) } }
    }

    fun useWav() {
        stopLiveInput()
        audioSource = AudioSource.Wav
        lastFeedNanos = 0L
        feedsWav = true
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    fun useMicrophone(audioSourceType: Int) {
        val input = AudioInput({ renderer.context }, audioSourceType)
        startLiveInput(AudioSource.Microphone, LiveInput(input::start, input::stop))
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    fun useSystemAudio(projection: MediaProjection) {
        val input = PlaybackCaptureInput(projection) { renderer.context }
        startLiveInput(AudioSource.SystemAudio, LiveInput(input::start, input::stop))
    }

    fun onPause() {
        surface?.onPause()
        renderer.pause()
        liveInput?.stop?.invoke()
        Log.i(TAG, "lifecycle pause")
    }

    fun onResume() {
        surface?.onResume()
        renderer.resume()
        lastFeedNanos = 0L
        liveInput?.let { restart(it) }
        Log.i(TAG, "lifecycle resume")
    }

    private fun startLiveInput(source: AudioSource, input: LiveInput) {
        stopLiveInput()
        feedsWav = false
        if (restart(input)) {
            liveInput = input
            audioSource = source
        } else {
            useWav()
        }
    }

    private fun restart(input: LiveInput): Boolean =
        runCatching { input.start() }
            .onFailure {
                Log.e(TAG, "audio input failed", it)
                errorMessage = it.message
            }
            .isSuccess

    private fun stopLiveInput() {
        liveInput?.stop?.invoke()
        liveInput = null
    }

    private fun feedWav() {
        val now = System.nanoTime()
        val last = lastFeedNanos
        lastFeedNanos = now
        val context = renderer.context ?: return
        if (!feedsWav) return
        val deltaSeconds = if (last == 0L) FRAME_SECONDS else ((now - last) / NANOS_PER_SECOND).coerceAtMost(MAX_FEED_SECONDS)
        wavFeeder?.feed(context, deltaSeconds)
    }

    private fun load(context: SynesthesiaContext, target: Preset, blendSeconds: Float) {
        runCatching { with(library) { context.loadPreset(target.name, blendSeconds) } }
            .onSuccess { Log.i(TAG, "preset loaded: ${target.name}") }
            .onFailure {
                Log.e(TAG, "preset failed: ${target.name}", it)
                errorMessage = it.message
            }
    }

    private class LiveInput(val start: () -> Unit, val stop: () -> Unit)

    private companion object {
        const val TAG = "SynesthesiaVibeSample"
        const val BLEND_SECONDS = 2f
        const val FRAME_SECONDS = 1.0 / 60.0
        const val MAX_FEED_SECONDS = 0.25
        const val NANOS_PER_SECOND = 1.0e9
    }
}
