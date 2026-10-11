//
//  CaptureRenderer.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.opengl.GLSurfaceView
import android.util.Log
import com.synesthesiavibe.CaptureSession
import com.synesthesiavibe.SynesthesiaConfiguration
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

sealed interface CaptureResult {
    data class Success(val files: List<File>) : CaptureResult
    data class Failure(val message: String) : CaptureResult
}

class CaptureRenderer(
    private val request: CaptureRequest,
    private val onFinished: (CaptureResult) -> Unit,
) : GLSurfaceView.Renderer {

    private val started = AtomicBoolean(false)

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = Unit

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = Unit

    override fun onDrawFrame(gl: GL10?) {
        if (!started.compareAndSet(false, true)) return
        val result = runCatching { capture() }.fold(
            onSuccess = { CaptureResult.Success(it) },
            onFailure = { CaptureResult.Failure(it.message ?: it.toString()) },
        )
        writeDoneMarker(request.outputDirectory, result)
        onFinished(result)
    }

    private fun capture(): List<File> {
        val configuration = SynesthesiaConfiguration.builder()
            .setResourcePath(request.resourceRoot)
            .build()
        val session = CaptureSession(configuration, request.preset.file, request.wav, request.frames.toIntArray(), request.outputDirectory)
        Log.i(TAG, "CAPTURE START preset=${request.preset.id ?: request.preset.name} frames=${request.frames.joinToString(",")} out=${request.outputDirectory}")
        session.run()
        return session.writtenFiles
    }

    companion object {

        private const val TAG = "SynesthesiaVibeSample"

        fun writeDoneMarker(outputDirectory: File, result: CaptureResult) {
            val line = when (result) {
                is CaptureResult.Success -> "CAPTURE OK files=${result.files.size} out=$outputDirectory"
                is CaptureResult.Failure -> "CAPTURE FAILED ${result.message}"
            }
            when (result) {
                is CaptureResult.Success -> Log.i(TAG, line)
                is CaptureResult.Failure -> Log.e(TAG, line)
            }
            runCatching {
                outputDirectory.mkdirs()
                File(outputDirectory, CaptureRequest.DONE_MARKER).writeText(line + "\n")
            }.onFailure { Log.e(TAG, "could not write ${CaptureRequest.DONE_MARKER} marker in $outputDirectory", it) }
        }
    }
}
