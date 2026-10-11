//
//  SampleLibrary.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.content.Context
import com.synesthesiavibe.PresetAssets
import com.synesthesiavibe.SynesthesiaContext
import org.json.JSONObject
import java.io.File

data class Preset(val id: String?, val file: File) {
    val name: String get() = file.nameWithoutExtension
}

class SampleLibrary private constructor(
    val resourceRoot: File,
    val presets: List<Preset>,
    val capturePresets: List<Preset>,
    val defaultWav: File?,
) {

    fun preset(key: String): Preset? = presets.firstOrNull { it.matches(key) }

    fun capturePreset(key: String): Preset? = (capturePresets + presets).firstOrNull { it.matches(key) }

    fun SynesthesiaContext.loadPreset(name: String, blendSeconds: Float = 0f) {
        val preset = preset(name) ?: capturePreset(name) ?: throw IllegalArgumentException("no bundled preset matches $name")
        loadPreset(preset.file, blendSeconds)
    }

    private fun Preset.matches(key: String): Boolean = id == key || file.name == key || name == key

    companion object {

        private const val CAPTURE_ROOT = "synesthesiavibe-capture"
        private const val CAPTURE_DIRECTORY = "capture-presets"
        private const val CAPTURE_SET_FILE = "capture-set.json"
        private const val DEFAULT_WAV = "deterministic-test.wav"
        private const val VERSION_FILE = ".assets-version"

        fun install(context: Context): SampleLibrary {
            val root = PresetAssets.install(context)
            val captureRoot = File(root.parentFile, CAPTURE_ROOT)
            installCaptureAssets(context, captureRoot)
            val ids = captureSetIds(File(captureRoot, CAPTURE_SET_FILE))
            return SampleLibrary(
                resourceRoot = root,
                presets = presets(PresetAssets.presetsDirectory(root), emptyMap()),
                capturePresets = presets(File(captureRoot, CAPTURE_DIRECTORY), ids),
                defaultWav = File(PresetAssets.audioDirectory(root), DEFAULT_WAV).takeIf { it.isFile },
            )
        }

        private fun installCaptureAssets(context: Context, captureRoot: File) {
            val version = context.packageManager.getPackageInfo(context.packageName, 0).lastUpdateTime.toString()
            val stamp = File(captureRoot, VERSION_FILE)
            if (stamp.isFile && stamp.readText() == version) return

            captureRoot.deleteRecursively()
            val presetsDirectory = File(captureRoot, CAPTURE_DIRECTORY).apply { mkdirs() }
            val assets = context.assets
            assets.list(CAPTURE_DIRECTORY).orEmpty().forEach { name ->
                assets.open("$CAPTURE_DIRECTORY/$name").use { input ->
                    File(presetsDirectory, name).outputStream().use { input.copyTo(it) }
                }
            }
            assets.open(CAPTURE_SET_FILE).use { input ->
                File(captureRoot, CAPTURE_SET_FILE).outputStream().use { input.copyTo(it) }
            }
            stamp.writeText(version)
        }

        private fun captureSetIds(file: File): Map<String, String> {
            if (!file.isFile) return emptyMap()
            val entries = JSONObject(file.readText()).getJSONArray("presets")
            return (0 until entries.length())
                .map { entries.getJSONObject(it) }
                .associate { it.getString("file") to it.getString("id") }
        }

        private fun presets(directory: File, ids: Map<String, String>): List<Preset> =
            directory.listFiles { file -> file.extension == "milk" }
                .orEmpty()
                .sortedBy { it.name.lowercase() }
                .map { Preset(ids[it.name], it) }
    }
}
