//
//  VisualizerController.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.Manifest
import android.app.Activity
import android.content.Context
import android.media.projection.MediaProjection
import android.net.Uri
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.synesthesiavibe.AudioInput
import com.synesthesiavibe.PlaybackCaptureInput
import com.synesthesiavibe.PreparedPreset
import com.synesthesiavibe.SynesthesiaConfiguration
import com.synesthesiavibe.SynesthesiaContext
import com.synesthesiavibe.SynesthesiaException
import com.synesthesiavibe.SynesthesiaRenderer
import com.synesthesiavibe.WavFeeder
import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

enum class AudioSource { None, Wav, Microphone, SystemAudio }

enum class SampleScreen { Visualizer, Playlist, Fullscreen }

class VisualizerController(
    private val context: Context,
    private val library: SampleLibrary,
    initialPreset: Preset,
    private val widthPixels: Int,
    private val heightPixels: Int,
    private val preserveEglContext: Boolean,
    initialFps: Float,
    private val maxLuminance: Float,
    private val flashLimit: Float,
    initialPlaybackRate: Float,
    private val validatePath: File?,
    private val validateText: String?,
    private val temporaryDirectory: File,
) {

    var screen by mutableStateOf(SampleScreen.Visualizer)
        private set
    var preset by mutableStateOf(initialPreset)
        private set
    var playlist by mutableStateOf(library.presets)
        private set
    var folderName by mutableStateOf(library.folderName)
        private set
    var audioSource by mutableStateOf(AudioSource.Wav)
        private set
    var errorMessage by mutableStateOf<String?>(null)
        private set
    var debugEnabled by mutableStateOf(false)
        private set
    var debugInfo by mutableStateOf<Map<String, Any?>?>(null)
        private set
    var playing by mutableStateOf(true)
        private set
    var fps by mutableStateOf(initialFps)
        private set
    var autoChangeEnabled by mutableStateOf(false)
        private set
    var secondsUntilChange by mutableStateOf(AUTO_CHANGE_SECONDS)
        private set
    var glVersion by mutableStateOf("")
        private set

    val showsSystemAudio: Boolean
        get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q

    val audioRate: Int?
        get() = when (audioSource) {
            AudioSource.Wav -> wavFeeder?.sampleRate
            AudioSource.Microphone, AudioSource.SystemAudio -> AudioInput.SAMPLE_RATE
            AudioSource.None -> null
        }

    private val mainHandler = Handler(Looper.getMainLooper())

    private val debugListener = SynesthesiaContext.DebugListener { info ->
        Log.i(TAG, "debug report thread=${Thread.currentThread().name} time=${info["time"]} frame=${info["frame"]} display_fps=${info["display_fps"]}")
        mainHandler.post {
            if (debugEnabled) debugInfo = info
        }
    }

    private var renderer: SynesthesiaRenderer = createRenderer(initialFps)
    private val wavFeeder: WavFeeder? = library.defaultWav?.let(::WavFeeder)

    private var attachedFps: Float? = null
    private var surface: GLSurfaceView? = null
    private var liveInput: LiveInput? = null

    @Volatile
    private var loadedPreset: Preset = initialPreset
    private val requestSequence = AtomicLong()
    private val pendingPrepared = AtomicReference<PreparedPreset?>()
    private val prepareExecutor = Executors.newSingleThreadExecutor { task ->
        Thread(null, task, "preset-prepare", PREPARE_STACK_BYTES).apply { isDaemon = true }
    }

    @Volatile
    private var feedsWav = true

    @Volatile
    private var lastFeedNanos = 0L

    private var playbackRate = initialPlaybackRate
    private var userPaused = false
    private var lifecyclePaused = false

    private val autoChangeTick = object : Runnable {
        override fun run() {
            if (autoChangeEnabled && playing && secondsUntilChange > 0) {
                secondsUntilChange -= 1
                if (secondsUntilChange == 0) selectPreset(stepPreset(1))
            }
            mainHandler.postDelayed(this, TICK_MILLIS)
        }
    }

    private val glRenderer = object : GLSurfaceView.Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = renderer.onSurfaceCreated(gl, config)

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = renderer.onSurfaceChanged(gl, width, height)

        override fun onDrawFrame(gl: GL10?) {
            feedWav()
            renderer.onDrawFrame(gl)
        }
    }

    fun attach(view: GLSurfaceView) {
        val previous = surface
        if (previous != null && previous !== view) {
            val retiring = renderer
            previous.queueEvent { retiring.release() }
            previous.onPause()
        }
        if (attachedFps != fps) {
            renderer = createRenderer(fps)
            attachedFps = fps
            applyRendererState()
        }
        SynesthesiaRenderer.configure(view)
        view.preserveEGLContextOnPause = preserveEglContext
        view.setRenderer(glRenderer)
        view.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        requestDisplayRate(view)
        surface = view
        mainHandler.removeCallbacks(autoChangeTick)
        mainHandler.postDelayed(autoChangeTick, TICK_MILLIS)
        Log.i(TAG, "surface attached preserveEGLContextOnPause=$preserveEglContext fps=$fps")
    }

    private fun requestDisplayRate(view: GLSurfaceView) {
        val activity = view.context as? Activity ?: return
        @Suppress("DEPRECATION")
        val display = activity.windowManager.defaultDisplay
        val current = display.mode
        val modes = display.supportedModes.filter {
            it.physicalWidth == current.physicalWidth && it.physicalHeight == current.physicalHeight
        }
        val fastest = modes.maxByOrNull { it.refreshRate } ?: return
        val mode = if (fps == FPS_UNCAPPED) {
            fastest
        } else {
            modes.filter { it.refreshRate >= fps - REFRESH_RATE_TOLERANCE }.minByOrNull { it.refreshRate } ?: fastest
        }
        val window = activity.window
        window.attributes = window.attributes.apply { preferredDisplayModeId = mode.modeId }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            view.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    holder.surface.setFrameRate(mode.refreshRate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT)
                }

                override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) = Unit

                override fun surfaceDestroyed(holder: SurfaceHolder) = Unit
            })
        }
        Log.i(TAG, "display mode requested fps=$fps refresh=${mode.refreshRate} modes=${modes.map { it.refreshRate }}")
    }

    fun detach() {
        stopLiveInput()
        mainHandler.removeCallbacks(autoChangeTick)
        requestSequence.incrementAndGet()
        pendingPrepared.getAndSet(null)?.close()
        val view = surface ?: return
        surface = null
        val retiring = renderer
        view.queueEvent {
            retiring.release()
            wavFeeder?.close()
        }
        view.onPause()
    }

    private fun createRenderer(maxFps: Float): SynesthesiaRenderer = SynesthesiaRenderer(
        SynesthesiaConfiguration.builder()
            .setSize(widthPixels, heightPixels)
            .setResourcePath(library.resourceRoot)
            .setMaxFps(maxFps)
            .setMaxLuminance(maxLuminance)
            .setFlashLimit(flashLimit)
            .build(),
        object : SynesthesiaRenderer.Listener {
            override fun onContextCreated(renderContext: SynesthesiaContext) {
                Log.i(TAG, "context created")
                glVersion = parseGlVersion(SynesthesiaContext.glStrings())
                validateRequested()
                load(renderContext, loadedPreset, 0f)
            }

            override fun onContextError(error: SynesthesiaException) {
                Log.e(TAG, "context error", error)
                errorMessage = error.message
            }
        },
    )

    private fun applyRendererState() {
        renderer.playbackRate = playbackRate
        renderer.debugInterval = DEBUG_INTERVAL_SECONDS
        renderer.debugListener = if (debugEnabled) debugListener else null
        if (userPaused || lifecyclePaused) renderer.pause()
    }

    fun reportError(message: String) {
        errorMessage = message
    }

    fun dismissError() {
        errorMessage = null
    }

    fun show(target: SampleScreen) {
        screen = target
    }

    fun back() {
        screen = SampleScreen.Visualizer
    }

    var importedFiles by mutableStateOf<Int?>(null)
        private set

    val importing: Boolean get() = importedFiles != null

    fun installFolder(uri: Uri) {
        if (importing) {
            Log.i(TAG, "folder install ignored, one is running: $uri")
            return
        }
        importedFiles = 0
        Log.i(TAG, "folder install start: $uri")
        val started = System.nanoTime()
        Thread {
            val installed = runCatching {
                library.installFolder(context, uri, temporaryDirectory) { copied ->
                    if (copied % IMPORT_PROGRESS_STEP == 0) mainHandler.post { if (importing) importedFiles = copied }
                }
            }
            mainHandler.post {
                importedFiles = null
                Log.i(TAG, "folder install finished in ${(System.nanoTime() - started) / NANOS_PER_MILLI} ms: ${installed.getOrNull()?.size ?: 0} presets")
                installed
                    .onFailure {
                        Log.e(TAG, "folder install failed", it)
                        errorMessage = it.message ?: "Folder install failed"
                    }
                    .onSuccess {
                        playlist = it
                        folderName = library.folderName
                        selectPreset(it.first())
                    }
            }
        }.start()
    }

    fun selectPreset(next: Preset) {
        preset = next
        loadedPreset = next
        resetCountdown()
        val ticket = requestSequence.incrementAndGet()
        val target = renderer.context ?: return
        prepareExecutor.execute { prepare(target, next, ticket) }
    }

    fun stepNext() = selectPreset(stepPreset(1))

    fun stepPrevious() = selectPreset(stepPreset(-1))

    private fun stepPreset(offset: Int): Preset {
        val list = playlist
        val index = list.indexOfFirst { it.file == preset.file }
        if (list.isEmpty() || index < 0) return preset
        return list[Math.floorMod(index + offset, list.size)]
    }

    fun applyPlaying(value: Boolean) {
        playing = value
        userPaused = !value
        if (value) renderer.resume() else renderer.pause()
        Log.i(TAG, "user playback playing=$value")
    }

    fun applyFps(value: Float) {
        if (value == fps) return
        fps = value
        Log.i(TAG, "fps $value")
    }

    fun setAutoChange(enabled: Boolean) {
        autoChangeEnabled = enabled
        resetCountdown()
        Log.i(TAG, "auto-change enabled=$enabled")
    }

    private fun resetCountdown() {
        secondsUntilChange = AUTO_CHANGE_SECONDS
    }

    fun applyDebugEnabled(enabled: Boolean) {
        debugEnabled = enabled
        renderer.debugListener = if (enabled) debugListener else null
        if (!enabled) debugInfo = null
        Log.i(TAG, "debug overlay enabled=$enabled")
    }

    fun useNone() {
        stopLiveInput()
        audioSource = AudioSource.None
        feedsWav = false
        lastFeedNanos = 0L
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
        lifecyclePaused = true
        surface?.onPause()
        renderer.pause()
        liveInput?.stop?.invoke()
        Log.i(TAG, "lifecycle pause")
    }

    fun onResume() {
        lifecyclePaused = false
        surface?.onResume()
        if (!userPaused) renderer.resume()
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
                errorMessage = it.message ?: "Audio input failed"
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
        val liveContext = renderer.context ?: return
        if (!feedsWav) return
        val deltaSeconds = if (last == 0L) FRAME_SECONDS else ((now - last) / NANOS_PER_SECOND).coerceAtMost(MAX_FEED_SECONDS)
        wavFeeder?.feed(liveContext, deltaSeconds)
    }

    private fun validateRequested() {
        validatePath?.let { path -> logValidation("path=${path.path}") { SynesthesiaContext.validatePreset(path) } }
        validateText?.let { text -> logValidation("text length=${text.length}") { SynesthesiaContext.validatePresetText(text, temporaryDirectory) } }
    }

    private fun logValidation(target: String, validate: () -> Unit) {
        runCatching(validate)
            .onSuccess { Log.i(TAG, "validate $target status=SV_OK") }
            .onFailure {
                val status = (it as? SynesthesiaException)?.status ?: it.javaClass.simpleName
                Log.i(TAG, "validate $target status=$status message=${it.message}")
            }
    }

    private fun load(target: SynesthesiaContext, wanted: Preset, blendSeconds: Float) {
        val started = System.nanoTime()
        runCatching { target.loadPreset(wanted.file, blendSeconds) }
            .onSuccess { Log.i(TAG, "preset loaded in ${(System.nanoTime() - started) / NANOS_PER_MILLI} ms: ${wanted.name}") }
            .onFailure {
                Log.e(TAG, "preset failed: ${wanted.name}", it)
                errorMessage = it.message
            }
    }

    private fun prepare(target: SynesthesiaContext, wanted: Preset, ticket: Long) {
        if (ticket != requestSequence.get()) return
        val started = System.nanoTime()
        val prepared = try {
            target.preparePreset(wanted.file)
        } catch (failure: Exception) {
            Log.e(TAG, "preset prepare failed: ${wanted.name}", failure)
            if (ticket == requestSequence.get()) mainHandler.post { errorMessage = failure.message }
            return
        }
        Log.i(TAG, "preset prepared in ${(System.nanoTime() - started) / NANOS_PER_MILLI} ms: ${wanted.name}")
        if (ticket != requestSequence.get()) {
            prepared.close()
            return
        }
        pendingPrepared.getAndSet(prepared)?.close()
        mainHandler.post {
            val view = surface
            if (view == null) {
                pendingPrepared.compareAndSet(prepared, null)
                prepared.close()
            } else {
                view.queueEvent { commit(target, prepared, wanted, ticket) }
            }
        }
    }

    private fun commit(target: SynesthesiaContext, prepared: PreparedPreset, wanted: Preset, ticket: Long) {
        if (!pendingPrepared.compareAndSet(prepared, null)) {
            prepared.close()
            return
        }
        try {
            if (ticket != requestSequence.get() || renderer.context !== target) return
            val started = System.nanoTime()
            runCatching { target.commitPreset(prepared, BLEND_SECONDS) }
                .onSuccess { Log.i(TAG, "preset loaded in ${(System.nanoTime() - started) / NANOS_PER_MILLI} ms: ${wanted.name}") }
                .onFailure {
                    Log.e(TAG, "preset failed: ${wanted.name}", it)
                    errorMessage = it.message
                }
        } finally {
            prepared.close()
        }
    }

    private class LiveInput(val start: () -> Unit, val stop: () -> Unit)

    companion object {

        private const val TAG = "SynesthesiaVibeSample"
        private const val TICK_MILLIS = 1000L
        private const val FRAME_SECONDS = 1.0 / 60.0
        private const val MAX_FEED_SECONDS = 0.25
        private const val NANOS_PER_SECOND = 1.0e9
        private const val REFRESH_RATE_TOLERANCE = 1f
        private const val NANOS_PER_MILLI = 1_000_000L
        private const val PREPARE_STACK_BYTES = 8L * 1024 * 1024
        private const val IMPORT_PROGRESS_STEP = 10
        private const val GL_VERSION_PREFIX = "GL_VERSION="

        const val BLEND_SECONDS = 2f
        const val AUTO_CHANGE_SECONDS = 30
        const val FPS_DEFAULT = 60f
        const val FPS_UNCAPPED = 0f
        const val DEBUG_INTERVAL_SECONDS = 1f

        fun parseGlVersion(strings: String): String = strings.lineSequence()
            .firstOrNull { it.startsWith(GL_VERSION_PREFIX) }
            ?.removePrefix(GL_VERSION_PREFIX)
            ?.trim()
            .orEmpty()
    }
}
