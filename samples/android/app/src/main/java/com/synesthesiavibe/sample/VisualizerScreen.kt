//
//  VisualizerScreen.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.opengl.GLSurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner

@Composable
fun VisualizerScreen(
    controller: VisualizerController,
    presets: List<Preset>,
    showsSystemAudio: Boolean,
    onSourceSelected: (AudioSource) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black),
    ) {
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
        ) {
            VisualizerSurface(controller = controller, modifier = Modifier.fillMaxSize())
            Text(
                text = controller.errorMessage ?: controller.preset.name,
                color = if (controller.errorMessage == null) Color.White else MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.titleSmall,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .statusBarsPadding()
                    .padding(16.dp)
                    .background(Color.Black.copy(alpha = 0.5f), MaterialTheme.shapes.small)
                    .padding(horizontal = 8.dp, vertical = 4.dp),
            )
            controller.debugInfo?.let { info ->
                Text(
                    text = debugLines(info).joinToString("\n"),
                    color = Color.White,
                    fontFamily = FontFamily.Monospace,
                    style = MaterialTheme.typography.labelSmall,
                    modifier = Modifier
                        .align(Alignment.BottomStart)
                        .padding(8.dp)
                        .background(Color.Black.copy(alpha = 0.6f), MaterialTheme.shapes.small)
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                )
            }
        }
        DebugControls(
            enabled = controller.debugEnabled,
            interval = controller.debugInterval,
            onToggle = controller::enableDebug,
            onCycleInterval = controller::cycleDebugInterval,
        )
        SourceSelector(
            selected = controller.audioSource,
            showsSystemAudio = showsSystemAudio,
            onSourceSelected = onSourceSelected,
        )
        HorizontalDivider()
        PresetList(
            presets = presets,
            selected = controller.preset,
            onPresetSelected = controller::selectPreset,
            modifier = Modifier
                .fillMaxWidth()
                .height(260.dp)
                .navigationBarsPadding(),
        )
    }
}

@Composable
private fun VisualizerSurface(controller: VisualizerController, modifier: Modifier = Modifier) {
    val lifecycleOwner = LocalLifecycleOwner.current

    DisposableEffect(lifecycleOwner, controller) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_PAUSE -> controller.onPause()
                Lifecycle.Event.ON_RESUME -> controller.onResume()
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
            controller.detach()
        }
    }

    AndroidView(
        modifier = modifier,
        factory = { context -> GLSurfaceView(context).also(controller::attach) },
    )
}

@Composable
private fun DebugControls(
    enabled: Boolean,
    interval: Float,
    onToggle: (Boolean) -> Unit,
    onCycleInterval: () -> Unit,
) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 4.dp),
    ) {
        FilterChip(
            selected = enabled,
            onClick = { onToggle(!enabled) },
            label = { Text("Debug info") },
        )
        FilterChip(
            selected = false,
            onClick = onCycleInterval,
            label = { Text("Every ${interval}s") },
        )
    }
}

private fun debugLines(info: Map<String, Any?>): List<String> = listOf(
    "time=${info["time"].asText()} frame=${info["frame"]}",
    "fps=${info["display_fps"].asText()} steps/frame=${info["sim_steps_per_frame"].asText()} rate=${info["playback_rate"].asText()}",
    "preset=${info["preset_name"]} v${info["preset_version"]}",
    "warp=${info["warp_shader"]} comp=${info["comp_shader"]} limiter=${info["limiter_active"]}",
    "bass=${info["bass"].asText()} mid=${info["mid"].asText()} treb=${info["treb"].asText()}",
    "att=${info["bass_att"].asText()} ${info["mid_att"].asText()} ${info["treb_att"].asText()}",
    "tex=${info["texsize_x"]}x${info["texsize_y"]} view=${info["width"]}x${info["height"]} fb=${info["feedback_format"]}",
    "gl_errors=${info["gl_errors"]} last_error=${info["last_error"]}",
)

private fun Any?.asText(): String = when (this) {
    is Number -> "%.2f".format(toDouble())
    else -> toString()
}

@Composable
private fun SourceSelector(
    selected: AudioSource,
    showsSystemAudio: Boolean,
    onSourceSelected: (AudioSource) -> Unit,
) {
    val sources = AudioSource.entries.filter { showsSystemAudio || it != AudioSource.SystemAudio }

    Row(
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
    ) {
        sources.forEach { source ->
            FilterChip(
                selected = source == selected,
                onClick = { onSourceSelected(source) },
                label = { Text(source.label) },
            )
        }
    }
}

@Composable
private fun PresetList(
    presets: List<Preset>,
    selected: Preset,
    onPresetSelected: (Preset) -> Unit,
    modifier: Modifier = Modifier,
) {
    LazyColumn(modifier = modifier) {
        items(presets, key = { it.file.path }) { preset ->
            val isSelected = preset.file == selected.file
            Text(
                text = preset.name,
                color = if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onPresetSelected(preset) }
                    .padding(horizontal = 16.dp, vertical = 12.dp),
            )
        }
    }
}

private val AudioSource.label: String
    get() = when (this) {
        AudioSource.Wav -> "WAV"
        AudioSource.Microphone -> "Microphone"
        AudioSource.SystemAudio -> "System audio"
    }
