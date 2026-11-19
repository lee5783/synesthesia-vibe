//
//  SweepRunner.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

package com.synesthesiavibe.sample

import android.opengl.GLES20
import android.os.Build
import android.os.PowerManager
import android.util.Log
import com.synesthesiavibe.PreparedPreset
import com.synesthesiavibe.SynesthesiaContext
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.util.concurrent.Executor
import java.util.concurrent.atomic.AtomicReference
import kotlin.math.ceil

class SweepRunner(
    private val request: SweepRequest,
    private val contextProvider: () -> SynesthesiaContext?,
    private val prepareExecutor: Executor,
    private val surfaceSize: () -> Pair<Int, Int>,
    private val displayHz: () -> Float,
    private val powerManager: PowerManager?,
    private val onPreset: (Preset) -> Unit,
) {

    private enum class Phase { Idle, Preparing, Measuring, Finished }

    private sealed interface Outcome {
        val ticket: Long
        val millis: Double

        class Ready(override val ticket: Long, override val millis: Double, val prepared: PreparedPreset) : Outcome
        class Failed(override val ticket: Long, override val millis: Double, val message: String) : Outcome
    }

    private class Readback(val hash: Long, val luminanceRange: Int)

    private val fpsWindowSeconds = request.dwellSeconds / 2
    private val costWindowSeconds = request.dwellSeconds - fpsWindowSeconds

    private var phase = Phase.Idle
    private var position = 0
    @Volatile
    private var ticket = 0L
    private val outcome = AtomicReference<Outcome?>()

    private var prepareMillis = 0.0
    private var commitMillis = 0.0
    private var lastError = ""
    private var measureStartNanos = 0L
    private var lastFrameStartNanos = 0L
    private val intervals = ArrayList<Long>()
    private val costs = ArrayList<Long>()
    private var firstReadback: Readback? = null
    private var reports = 0
    private var glErrors = 0
    private var simStepsPerFrame: Float? = null
    private var warpShader: Int? = null
    private var compShader: Int? = null
    private var reportLastError = ""
    private var reportDisplayFps: Float? = null
    private var surfaceWidth: Int? = null
    private var surfaceHeight: Int? = null
    private var texsizeX: Int? = null
    private var texsizeY: Int? = null
    private var readbackBuffer: ByteBuffer? = null

    val debugListener = SynesthesiaContext.DebugListener { info ->
        val current = request.entries.getOrNull(position)?.preset ?: return@DebugListener
        if (phase != Phase.Measuring || info["preset_name"] != current.name) return@DebugListener
        reports += 1
        glErrors += (info["gl_errors"] as? Int) ?: 0
        simStepsPerFrame = info["sim_steps_per_frame"] as? Float
        warpShader = info["warp_shader"] as? Int
        compShader = info["comp_shader"] as? Int
        reportLastError = (info["last_error"] as? String).orEmpty()
        reportDisplayFps = info["display_fps"] as? Float
        surfaceWidth = info["width"] as? Int
        surfaceHeight = info["height"] as? Int
        texsizeX = info["texsize_x"] as? Int
        texsizeY = info["texsize_y"] as? Int
    }

    fun onContext() {
        when (phase) {
            Phase.Finished -> Unit
            Phase.Idle -> begin()
            Phase.Preparing, Phase.Measuring -> startPreset()
        }
    }

    fun draw(render: () -> Unit) {
        val frameStart = System.nanoTime()
        advanceBeforeFrame()
        if (phase != Phase.Measuring) {
            render()
            return
        }
        val elapsed = (frameStart - measureStartNanos) / NANOS_PER_SECOND
        if (elapsed < request.warmupSeconds + fpsWindowSeconds) {
            render()
            advanceAfterPlainFrame(frameStart, elapsed)
            return
        }
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0)
        GLES20.glClearColor(0f, 0f, 0f, 0f)
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
        GLES20.glFinish()
        val costStart = System.nanoTime()
        render()
        GLES20.glFinish()
        advanceAfterCostFrame(costStart, System.nanoTime(), elapsed)
    }

    private fun begin() {
        request.outputDirectory.mkdirs()
        File(request.outputDirectory, SweepRequest.DONE_FILE).delete()
        File(request.outputDirectory, SweepRequest.STARTED_FILE).delete()
        writeMeta()
        Log.i(TAG, "SWEEP START presets=${request.entries.size} library=${request.libraryCount} out=${request.outputDirectory}")
        if (request.entries.isEmpty()) finish() else startPreset()
    }

    private fun startPreset() {
        val context = contextProvider() ?: return
        val entry = request.entries[position]
        val myTicket = ++ticket
        phase = Phase.Preparing
        outcome.set(null)
        intervals.clear()
        costs.clear()
        firstReadback = null
        reports = 0
        glErrors = 0
        simStepsPerFrame = null
        warpShader = null
        compShader = null
        reportLastError = ""
        reportDisplayFps = null
        surfaceWidth = null
        surfaceHeight = null
        texsizeX = null
        texsizeY = null
        lastError = ""
        prepareMillis = 0.0
        commitMillis = 0.0
        writeStarted(entry)
        onPreset(entry.preset)
        Log.i(TAG, "SWEEP PRESET index=${entry.index} name=${entry.preset.name}")
        prepareExecutor.execute {
            val started = System.nanoTime()
            val result = try {
                val prepared = context.preparePreset(entry.preset.file)
                Outcome.Ready(myTicket, millisSince(started), prepared)
            } catch (failure: Throwable) {
                Outcome.Failed(myTicket, millisSince(started), failure.message ?: failure.toString())
            }
            if (ticket == myTicket) outcome.set(result) else (result as? Outcome.Ready)?.prepared?.close()
        }
    }

    private fun advanceBeforeFrame() {
        if (phase != Phase.Preparing) return
        val ready = outcome.get() ?: return
        if (ready.ticket != ticket) return
        outcome.set(null)
        prepareMillis = ready.millis
        when (ready) {
            is Outcome.Failed -> {
                record(status = "load_error", error = ready.message, commitMs = null, measured = false)
                next()
            }
            is Outcome.Ready -> commit(ready.prepared)
        }
    }

    private fun commit(prepared: PreparedPreset) {
        val context = contextProvider()
        if (context == null) {
            prepared.close()
            return
        }
        val started = System.nanoTime()
        val failure = try {
            context.commitPreset(prepared, 0f)
            null
        } catch (error: Throwable) {
            error.message ?: error.toString()
        } finally {
            prepared.close()
        }
        commitMillis = millisSince(started)
        if (failure != null) {
            record(status = "load_error", error = failure, commitMs = commitMillis, measured = false)
            next()
            return
        }
        lastError = context.lastError().orEmpty()
        context.setDebugInterval(DEBUG_INTERVAL_SECONDS)
        lastFrameStartNanos = 0L
        measureStartNanos = System.nanoTime()
        phase = Phase.Measuring
    }

    private fun advanceAfterPlainFrame(frameStart: Long, elapsed: Double) {
        val previousStart = lastFrameStartNanos
        lastFrameStartNanos = frameStart
        if (elapsed < request.warmupSeconds) return
        if (previousStart != 0L) intervals += frameStart - previousStart
        if (firstReadback == null) {
            firstReadback = readback()
            lastFrameStartNanos = 0L
        }
    }

    private fun advanceAfterCostFrame(costStart: Long, frameEnd: Long, elapsed: Double) {
        costs += frameEnd - costStart
        if (elapsed >= request.warmupSeconds + request.dwellSeconds) {
            val lastReadback = readback()
            finishMeasure(firstReadback, lastReadback)
            next()
        }
    }

    private fun finishMeasure(first: Readback?, last: Readback?) {
        val blank = first != null && last != null && first.luminanceRange <= BLANK_LUMINANCE_RANGE && last.luminanceRange <= BLANK_LUMINANCE_RANGE
        val frozen = first != null && last != null && !blank && first.hash == last.hash
        val degraded = lastError.isNotEmpty() || reportLastError.isNotEmpty() || glErrors > 0
        record(
            status = if (degraded) "degraded" else "ok",
            error = lastError.ifEmpty { reportLastError },
            commitMs = commitMillis,
            measured = true,
            blank = blank,
            frozen = frozen,
        )
    }

    private fun next() {
        position += 1
        if (position >= request.entries.size) finish() else startPreset()
    }

    private fun finish() {
        phase = Phase.Finished
        val line = "SWEEP OK count=${request.entries.size}"
        Log.i(TAG, line)
        runCatching { File(request.outputDirectory, SweepRequest.DONE_FILE).writeText(line + "\n") }
            .onFailure { Log.e(TAG, "could not write ${SweepRequest.DONE_FILE} marker", it) }
    }

    private fun record(status: String, error: String, commitMs: Double?, measured: Boolean, blank: Boolean = false, frozen: Boolean = false) {
        val entry = request.entries[position]
        val sortedIntervals = intervals.sorted()
        val sortedCosts = costs.sorted()
        val totalIntervalNanos = intervals.sum()
        val json = JSONObject()
            .put("index", entry.index)
            .put("name", entry.preset.name)
            .put("source", entry.source)
            .put("status", status)
            .put("error", error)
            .put("prepare_ms", round(prepareMillis))
            .put("commit_ms", commitMs?.let(::round) ?: JSONObject.NULL)
            .put("frames", if (measured) intervals.size + costs.size else JSONObject.NULL)
            .put("fps_frames", if (measured) intervals.size else JSONObject.NULL)
            .put("cost_frames", if (measured) costs.size else JSONObject.NULL)
            .put("fps_mean", if (measured && totalIntervalNanos > 0) round(intervals.size * NANOS_PER_SECOND / totalIntervalNanos) else JSONObject.NULL)
            .put("fps_p5", if (measured && sortedIntervals.isNotEmpty()) round(NANOS_PER_SECOND / percentile(sortedIntervals, 0.95)) else JSONObject.NULL)
            .put("cost_p50_ms", if (measured && sortedCosts.isNotEmpty()) round(percentile(sortedCosts, 0.5) / NANOS_PER_MILLI) else JSONObject.NULL)
            .put("cost_p95_ms", if (measured && sortedCosts.isNotEmpty()) round(percentile(sortedCosts, 0.95) / NANOS_PER_MILLI) else JSONObject.NULL)
            .put("sim_steps_per_frame", if (measured) simStepsPerFrame?.let { round(it.toDouble()) } ?: JSONObject.NULL else JSONObject.NULL)
            .put("warp_shader", if (measured) warpShader ?: JSONObject.NULL else JSONObject.NULL)
            .put("comp_shader", if (measured) compShader ?: JSONObject.NULL else JSONObject.NULL)
            .put("gl_errors", if (measured) glErrors else JSONObject.NULL)
            .put("last_error", if (measured) lastError.ifEmpty { reportLastError } else JSONObject.NULL)
            .put("thermal_status", thermalStatus())
            .put("display_hz", round(displayHz().toDouble()))
            .put("blank", if (measured) blank else JSONObject.NULL)
            .put("frozen", if (measured) frozen else JSONObject.NULL)
            .put("report_display_fps", reportDisplayFps?.let { round(it.toDouble()) } ?: JSONObject.NULL)
            .put("reports", reports)
            .put("warmup_s", request.warmupSeconds.toDouble())
            .put("measure_s", request.dwellSeconds.toDouble())
            .put("fps_window_s", fpsWindowSeconds.toDouble())
            .put("cost_window_s", costWindowSeconds.toDouble())
            .put("surface_w", surfaceWidth ?: JSONObject.NULL)
            .put("surface_h", surfaceHeight ?: JSONObject.NULL)
            .put("texsize_x", texsizeX ?: JSONObject.NULL)
            .put("texsize_y", texsizeY ?: JSONObject.NULL)
            .put("set", request.setName ?: JSONObject.NULL)
            .put("timestamp_ms", System.currentTimeMillis())
        appendLine(File(request.outputDirectory, SweepRequest.RESULTS_FILE), json.toString())
        Log.i(TAG, "SWEEP RESULT index=${entry.index} status=$status name=${entry.preset.name}")
    }

    private fun readback(): Readback? {
        val (width, height) = surfaceSize()
        if (width <= 0 || height <= 0) return null
        val buffer = readbackBuffer?.takeIf { it.capacity() >= width * BYTES_PER_PIXEL }
            ?: ByteBuffer.allocateDirect(width * BYTES_PER_PIXEL).also { readbackBuffer = it }
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0)
        var hash = FNV_OFFSET
        var minimum = Int.MAX_VALUE
        var maximum = 0
        ROW_FRACTIONS.forEach { fraction ->
            val y = ((height - 1) * fraction).toInt()
            buffer.clear()
            GLES20.glReadPixels(0, y, width, 1, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, buffer)
            for (offset in 0 until width * BYTES_PER_PIXEL) {
                hash = (hash xor (buffer.get(offset).toLong() and 0xff)) * FNV_PRIME
            }
            for (pixel in 0 until width step PIXEL_STRIDE) {
                val base = pixel * BYTES_PER_PIXEL
                val luminance = ((buffer.get(base).toInt() and 0xff) * 3 + (buffer.get(base + 1).toInt() and 0xff) * 6 + (buffer.get(base + 2).toInt() and 0xff)) / 10
                if (luminance < minimum) minimum = luminance
                if (luminance > maximum) maximum = luminance
            }
        }
        while (GLES20.glGetError() != GLES20.GL_NO_ERROR) Unit
        return Readback(hash, maximum - minimum)
    }

    private fun thermalStatus(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) powerManager?.currentThermalStatus ?: -1 else -1

    private fun writeStarted(entry: SweepEntry) {
        val target = File(request.outputDirectory, SweepRequest.STARTED_FILE)
        val staging = File(request.outputDirectory, "${SweepRequest.STARTED_FILE}.tmp")
        runCatching {
            staging.writeText("${entry.index}\t${entry.preset.name}\t${System.currentTimeMillis()}\t${entry.source}\n")
            staging.renameTo(target)
        }.onFailure { Log.e(TAG, "could not write ${SweepRequest.STARTED_FILE} marker", it) }
    }

    private fun writeMeta() {
        val json = JSONObject()
            .put("model", Build.MODEL)
            .put("device", Build.DEVICE)
            .put("sdk_int", Build.VERSION.SDK_INT)
            .put("fingerprint", Build.FINGERPRINT)
            .put("pid", android.os.Process.myPid())
            .put("set", request.setName ?: JSONObject.NULL)
            .put("library_count", request.libraryCount)
            .put("run_count", request.entries.size)
            .put("first_index", request.entries.firstOrNull()?.index ?: JSONObject.NULL)
            .put("warmup_s", request.warmupSeconds.toDouble())
            .put("measure_s", request.dwellSeconds.toDouble())
            .put("display_hz", round(displayHz().toDouble()))
            .put("timestamp_ms", System.currentTimeMillis())
        runCatching { File(request.outputDirectory, SweepRequest.META_FILE).writeText(json.toString() + "\n") }
            .onFailure { Log.e(TAG, "could not write ${SweepRequest.META_FILE}", it) }
    }

    private fun appendLine(file: File, line: String) {
        runCatching {
            FileOutputStream(file, true).use { stream ->
                stream.write((line + "\n").toByteArray(Charsets.UTF_8))
                stream.flush()
                stream.fd.sync()
            }
        }.onFailure { Log.e(TAG, "could not append to ${file.path}", it) }
    }

    private fun millisSince(startNanos: Long): Double = (System.nanoTime() - startNanos) / NANOS_PER_MILLI

    private fun round(value: Double): Double = Math.round(value * ROUND_SCALE) / ROUND_SCALE

    private fun percentile(sorted: List<Long>, fraction: Double): Double =
        sorted[(ceil(fraction * sorted.size).toInt() - 1).coerceIn(0, sorted.size - 1)].toDouble()

    private companion object {
        const val TAG = "SynesthesiaVibeSample"
        const val NANOS_PER_SECOND = 1.0e9
        const val NANOS_PER_MILLI = 1.0e6
        const val ROUND_SCALE = 1000.0
        const val DEBUG_INTERVAL_SECONDS = 1f
        const val BYTES_PER_PIXEL = 4
        const val PIXEL_STRIDE = 8
        const val BLANK_LUMINANCE_RANGE = 4
        const val FNV_OFFSET = -3750763034362895579L
        const val FNV_PRIME = 1099511628211L
        val ROW_FRACTIONS = listOf(0.15, 0.35, 0.5, 0.65, 0.85)
    }
}
