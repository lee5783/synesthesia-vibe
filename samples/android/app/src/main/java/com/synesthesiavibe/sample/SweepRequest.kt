//
//  SweepRequest.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe.sample

import android.content.Context
import android.content.Intent
import org.json.JSONObject
import java.io.File

data class SweepEntry(val index: Int, val preset: Preset, val source: String)

data class SweepRequest(
    val entries: List<SweepEntry>,
    val libraryCount: Int,
    val setName: String?,
    val outputDirectory: File,
    val warmupSeconds: Float,
    val dwellSeconds: Float,
) {

    companion object {

        const val EXTRA_SWEEP = "sweep"
        const val EXTRA_OUT = "sweepOut"
        const val EXTRA_START = "sweepStart"
        const val EXTRA_LIMIT = "sweepLimit"
        const val EXTRA_LIST = "sweepList"
        const val EXTRA_FILTER = "sweepFilter"
        const val EXTRA_WARMUP = "sweepWarmup"
        const val EXTRA_DWELL = "sweepDwell"

        const val RESULTS_FILE = "results.jsonl"
        const val STARTED_FILE = "started"
        const val DONE_FILE = "done"
        const val META_FILE = "meta.json"

        private const val DEFAULT_OUT = "sweep"
        private const val DEFAULT_WARMUP_SECONDS = 1f
        private const val DEFAULT_DWELL_SECONDS = 6f
        private const val SET_MANIFEST = "preset-set.json"
        private const val PRESET_EXTENSION = ".milk"

        fun isRequested(intent: Intent): Boolean = intent.getBooleanExtra(EXTRA_SWEEP, false)

        fun outputDirectory(context: Context, intent: Intent): File = resolve(outputBase(context), intent.getStringExtra(EXTRA_OUT) ?: DEFAULT_OUT)

        fun from(context: Context, intent: Intent, library: SampleLibrary): SweepRequest {
            val manifest = setManifest(context)
            val sources = manifest?.optJSONObject("presets")
            val all = library.presets.mapIndexed { index, preset ->
                SweepEntry(index, preset, sources?.optString(preset.file.name)?.takeIf { it.isNotEmpty() } ?: preset.file.name)
            }
            val start = start(intent.getStringExtra(EXTRA_START), all)
            val names = intent.getStringExtra(EXTRA_LIST)?.let { names(resolve(context.filesDir, it)) }
            val filter = intent.getStringExtra(EXTRA_FILTER)?.takeIf { it.isNotBlank() }?.lowercase()
            val limit = intent.getIntExtra(EXTRA_LIMIT, Int.MAX_VALUE).also { require(it > 0) { "invalid $EXTRA_LIMIT $it" } }
            val entries = all.asSequence()
                .filter { it.index >= start }
                .filter { names == null || it.preset.file.name.removeSuffix(PRESET_EXTENSION) in names }
                .filter { filter == null || it.preset.file.name.lowercase().contains(filter) }
                .take(limit)
                .toList()
            return SweepRequest(
                entries = entries,
                libraryCount = all.size,
                setName = manifest?.optString("name")?.takeIf { it.isNotEmpty() },
                outputDirectory = outputDirectory(context, intent),
                warmupSeconds = seconds(intent, EXTRA_WARMUP, DEFAULT_WARMUP_SECONDS, 0f),
                dwellSeconds = seconds(intent, EXTRA_DWELL, DEFAULT_DWELL_SECONDS, 0.5f),
            )
        }

        private fun outputBase(context: Context): File = context.getExternalFilesDir(null) ?: context.filesDir

        private fun resolve(base: File, path: String): File = File(path).takeIf { it.isAbsolute } ?: File(base, path)

        private fun seconds(intent: Intent, key: String, default: Float, minimum: Float): Float =
            intent.getFloatExtra(key, default).also { require(it >= minimum) { "invalid $key $it" } }

        private fun start(value: String?, all: List<SweepEntry>): Int {
            if (value.isNullOrBlank()) return 0
            value.toIntOrNull()?.let { return it.coerceAtLeast(0) }
            return all.firstOrNull { it.preset.file.name == value || it.preset.name == value }?.index
                ?: throw IllegalArgumentException("no preset matches $EXTRA_START $value")
        }

        private fun names(file: File): Set<String> {
            require(file.isFile) { "$EXTRA_LIST file not found at ${file.path}" }
            return file.readLines().map { it.trim().removeSuffix(PRESET_EXTENSION) }.filter { it.isNotEmpty() }.toSet()
        }

        private fun setManifest(context: Context): JSONObject? = runCatching {
            context.assets.open(SET_MANIFEST).use { JSONObject(it.readBytes().toString(Charsets.UTF_8)) }
        }.getOrNull()
    }
}
