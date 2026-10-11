//
//  MainActivity.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.lifecycle.lifecycleScope
import com.synesthesiavibe.AudioInput
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import java.io.File

class MainActivity : ComponentActivity() {

    private lateinit var controller: VisualizerController
    private var pendingRecordAudio: (() -> Unit)? = null

    private val recordAudioPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        val action = pendingRecordAudio
        pendingRecordAudio = null
        if (granted) action?.invoke() else Log.w(TAG, "RECORD_AUDIO denied")
    }

    private val projectionConsent = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        val data = result.data
        if (result.resultCode != Activity.RESULT_OK || data == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            Log.w(TAG, "MediaProjection consent declined")
            return@registerForActivityResult
        }
        ProjectionService.start(this, result.resultCode, data)
        lifecycleScope.launch {
            ProjectionService.projection.filterNotNull().first()
            startSystemAudio()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        if (CaptureRequest.isRequested(intent)) showCapture() else showVisualizer()
    }

    private fun showCapture() {
        val outputDirectory = CaptureRequest.outputDirectory(this, intent)
        File(outputDirectory, CaptureRequest.DONE_MARKER).delete()
        val request = runCatching { CaptureRequest.from(this, intent, SampleLibrary.install(this)) }.getOrElse {
            CaptureRenderer.writeDoneMarker(outputDirectory, CaptureResult.Failure(it.message ?: it.toString()))
            finish()
            return
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContent {
            CaptureScreen(request) { runOnUiThread { finish() } }
        }
    }

    private fun showVisualizer() {
        val library = SampleLibrary.install(this)
        val initialPreset = intent.getStringExtra(EXTRA_PRESET)?.let { key -> library.preset(key) ?: library.capturePreset(key)?.let { library.preset(it.file.name) } }
            ?: library.presets.first()
        val metrics = resources.displayMetrics
        controller = VisualizerController(
            library = library,
            initialPreset = initialPreset,
            widthPixels = metrics.widthPixels,
            heightPixels = metrics.heightPixels,
            preserveEglContext = intent.getBooleanExtra(EXTRA_PRESERVE_EGL_CONTEXT, true),
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) observeProjection()
        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                VisualizerScreen(
                    controller = controller,
                    presets = library.presets,
                    showsSystemAudio = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q,
                    onSourceSelected = ::selectSource,
                )
            }
        }
    }

    private fun selectSource(source: AudioSource) {
        val leavesSystemAudio = controller.audioSource == AudioSource.SystemAudio && source != AudioSource.SystemAudio
        when (source) {
            AudioSource.Wav -> controller.useWav()
            AudioSource.Microphone -> withRecordAudio(::startMicrophone)
            AudioSource.SystemAudio -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) withRecordAudio(::requestSystemAudio)
        }
        if (leavesSystemAudio && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) ProjectionService.stop(this)
    }

    private fun withRecordAudio(action: () -> Unit) {
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            action()
        } else {
            pendingRecordAudio = action
            recordAudioPermission.launch(Manifest.permission.RECORD_AUDIO)
        }
    }

    private fun startMicrophone() {
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) return
        controller.useMicrophone(AudioInput.preferredAudioSource(this))
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun requestSystemAudio() {
        if (ProjectionService.projection.value != null) {
            startSystemAudio()
        } else {
            projectionConsent.launch(getSystemService(MediaProjectionManager::class.java).createScreenCaptureIntent())
        }
    }

    private fun startSystemAudio() {
        val projection = ProjectionService.projection.value ?: return
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) return
        controller.useSystemAudio(projection)
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun observeProjection() {
        lifecycleScope.launch {
            ProjectionService.projection.collect { projection ->
                if (projection == null && controller.audioSource == AudioSource.SystemAudio) controller.useWav()
            }
        }
    }

    private companion object {
        const val TAG = "SynesthesiaVibeSample"
        const val EXTRA_PRESET = "preset"
        const val EXTRA_PRESERVE_EGL_CONTEXT = "preserve-egl-context"
    }
}
