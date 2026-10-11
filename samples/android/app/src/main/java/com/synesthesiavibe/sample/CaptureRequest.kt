//
//  CaptureRequest.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.content.Context
import android.content.Intent
import java.io.File

data class CaptureRequest(
    val preset: Preset,
    val wav: File?,
    val frames: List<Int>,
    val outputDirectory: File,
    val resourceRoot: File,
) {

    companion object {

        const val EXTRA_CAPTURE = "capture"
        const val EXTRA_FRAMES = "frames"
        const val EXTRA_OUT = "out"
        const val EXTRA_WAV = "wav"
        const val DONE_MARKER = "done"

        private const val DEFAULT_OUT = "capture"
        private const val NO_WAV = "none"
        private val STANDARD_FRAMES = listOf(0, 30, 60, 150, 300, 450, 599)

        fun isRequested(intent: Intent): Boolean = intent.hasExtra(EXTRA_CAPTURE)

        fun outputDirectory(context: Context, intent: Intent): File = resolve(outputBase(context), intent.getStringExtra(EXTRA_OUT) ?: DEFAULT_OUT)

        fun from(context: Context, intent: Intent, library: SampleLibrary): CaptureRequest {
            val key = intent.getStringExtra(EXTRA_CAPTURE)?.takeIf { it.isNotBlank() }
                ?: throw IllegalArgumentException("missing value for $EXTRA_CAPTURE")
            val preset = library.capturePreset(key) ?: throw IllegalArgumentException("no bundled preset matches $key")
            return CaptureRequest(
                preset = preset,
                wav = wav(intent.getStringExtra(EXTRA_WAV), library),
                frames = intent.getStringExtra(EXTRA_FRAMES)?.let(::frames) ?: STANDARD_FRAMES,
                outputDirectory = outputDirectory(context, intent),
                resourceRoot = library.resourceRoot,
            )
        }

        private fun outputBase(context: Context): File = context.getExternalFilesDir(null) ?: context.filesDir

        private fun resolve(base: File, path: String): File = File(path).takeIf { it.isAbsolute } ?: File(base, path)

        private fun frames(list: String): List<Int> {
            val parts = list.split(',').map { it.trim() }
            val frames = parts.mapNotNull { it.toIntOrNull() }
            require(frames.isNotEmpty() && frames.size == parts.size && frames.all { it >= 0 }) { "invalid $EXTRA_FRAMES list $list" }
            return frames.sorted()
        }

        private fun wav(value: String?, library: SampleLibrary): File? = when (value) {
            null -> library.defaultWav
            NO_WAV -> null
            else -> resolve(library.resourceRoot, value).also { require(it.isFile) { "WAV not found at ${it.path}" } }
        }
    }
}
