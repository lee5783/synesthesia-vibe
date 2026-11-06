//
//  SampleLibrary.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document
import androidx.documentfile.provider.DocumentFile
import com.synesthesiavibe.PresetAssets
import org.json.JSONObject
import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

data class Preset(val id: String?, val file: File) {
    val name: String get() = file.nameWithoutExtension
}

class SampleLibrary private constructor(
    val resourceRoot: File,
    val presets: List<Preset>,
    val capturePresets: List<Preset>,
    val defaultWav: File?,
) {

    var folderName: String = BUNDLED_FOLDER
        private set

    fun preset(key: String): Preset? = presets.firstOrNull { it.matches(key) }

    fun capturePreset(key: String): Preset? = (capturePresets + presets).firstOrNull { it.matches(key) }

    fun installFolder(context: Context, uri: Uri, cacheRoot: File, onCopied: (Int) -> Unit): List<Preset> {
        val source = DocumentFile.fromTreeUri(context, uri) ?: throw IllegalStateException("the chosen folder is not readable")
        val staging = File(cacheRoot, STAGING_FOLDER_DIRECTORY)
        staging.deleteRecursively()
        if (!staging.mkdirs()) throw IllegalStateException("could not create $staging")
        try {
            val files = mutableListOf<Pair<Uri, File>>()
            collectDocumentTree(context, uri, DocumentsContract.getTreeDocumentId(uri), staging, true, files)
            copyDocuments(context, files, onCopied)
            if (presets(staging, emptyMap()).isEmpty()) {
                throw IllegalStateException("no .milk presets in ${source.name ?: "the chosen folder"}")
            }
        } catch (error: Throwable) {
            staging.deleteRecursively()
            throw error
        }
        val destination = File(cacheRoot, PICKED_FOLDER_DIRECTORY)
        destination.deleteRecursively()
        if (!staging.renameTo(destination)) {
            staging.deleteRecursively()
            throw IllegalStateException("could not install $destination")
        }
        folderName = source.name ?: PICKED_FOLDER_DIRECTORY
        return presets(destination, emptyMap())
    }

    private fun collectDocumentTree(context: Context, tree: Uri, documentId: String, destination: File, topLevel: Boolean, files: MutableList<Pair<Uri, File>>) {
        destination.mkdirs()
        val children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId)
        val columns = arrayOf(Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE)
        val entries = context.contentResolver.query(children, columns, null, null, null)?.use { cursor ->
            buildList { while (cursor.moveToNext()) add(Triple(cursor.getString(0), cursor.getString(1), cursor.getString(2))) }
        }.orEmpty()
        entries.forEach { (id, name, mimeType) ->
            if (id == null || name == null || name == "." || name == ".." || name.contains('/')) return@forEach
            if (mimeType == Document.MIME_TYPE_DIR) {
                collectDocumentTree(context, tree, id, File(destination, name), false, files)
                return@forEach
            }
            val extension = name.substringAfterLast('.', "").lowercase()
            if (extension !in COPIED_EXTENSIONS || (!topLevel && extension == PRESET_EXTENSION)) return@forEach
            files += DocumentsContract.buildDocumentUriUsingTree(tree, id) to File(destination, name)
        }
    }

    private fun copyDocuments(context: Context, files: List<Pair<Uri, File>>, onCopied: (Int) -> Unit) {
        val copied = AtomicInteger()
        val pool = Executors.newFixedThreadPool(COPY_THREADS)
        try {
            files.map { (source, target) ->
                pool.submit {
                    context.contentResolver.openInputStream(source)?.use { input ->
                        target.outputStream().use { input.copyTo(it) }
                    }
                    onCopied(copied.incrementAndGet())
                }
            }.forEach { it.get() }
        } finally {
            pool.shutdownNow()
        }
    }

    private fun Preset.matches(key: String): Boolean = id == key || file.name == key || name == key

    companion object {

        private const val CAPTURE_ROOT = "synesthesiavibe-capture"
        private const val CAPTURE_DIRECTORY = "capture-presets"
        private const val CAPTURE_SET_FILE = "capture-set.json"
        private const val DEFAULT_WAV = "deterministic-test.wav"
        private const val VERSION_FILE = ".assets-version"
        private const val BUNDLED_FOLDER = "mvp-presets"
        private const val PICKED_FOLDER_DIRECTORY = "picked-presets"
        private const val STAGING_FOLDER_DIRECTORY = "picked-presets-staging"
        private const val PRESET_EXTENSION = "milk"
        private const val COPY_THREADS = 4
        private val COPIED_EXTENSIONS = setOf(PRESET_EXTENSION, "png", "jpg", "jpeg", "tga", "bmp", "gif", "dds")

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
