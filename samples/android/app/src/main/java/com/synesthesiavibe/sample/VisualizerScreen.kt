//
//  VisualizerScreen.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.app.Activity
import android.content.pm.ActivityInfo
import android.opengl.GLSurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilledIconToggleButton
import androidx.compose.material3.FilledTonalIconButton
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.RadioButtonDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner

private val FpsOptions = listOf(
    Triple(30f, "30", "30 fps"),
    Triple(60f, "60", "60 fps"),
    Triple(120f, "120", "120 fps"),
    Triple(VisualizerController.FPS_UNCAPPED, "Max", "Uncapped"),
)

private const val AUTO_CHANGE_SECONDS = 30
private const val DISABLED_ALPHA = 0.45f

@Composable
fun SampleApp(
    controller: VisualizerController,
    showsSystemAudio: Boolean,
    onSourceSelected: (AudioSource) -> Unit,
    onPickFolder: () -> Unit,
) {
    LifecycleBridge(controller)
    when (controller.screen) {
        SampleScreen.Visualizer -> VisualizerScreen(controller, onSourceSelected)
        SampleScreen.Playlist -> PlaylistScreen(controller, onPickFolder)
        SampleScreen.Fullscreen -> FullscreenScreen(controller)
    }
}

@Composable
private fun LifecycleBridge(controller: VisualizerController) {
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
}

@Composable
private fun errorSnackbarState(controller: VisualizerController): SnackbarHostState {
    val state = remember { SnackbarHostState() }
    val message = controller.errorMessage
    LaunchedEffect(message) {
        if (message != null) {
            state.showSnackbar(message)
            controller.dismissError()
        }
    }
    return state
}

@Composable
private fun ErrorSnackbarHost(state: SnackbarHostState) {
    SnackbarHost(state) { data ->
        Snackbar(data, containerColor = SvColors.DangerSubtle, contentColor = SvColors.Danger)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun VisualizerScreen(
    controller: VisualizerController,
    onSourceSelected: (AudioSource) -> Unit,
) {
    val snackbarHostState = errorSnackbarState(controller)
    Scaffold(
        containerColor = SvColors.BackgroundPrimary,
        snackbarHost = { ErrorSnackbarHost(snackbarHostState) },
        topBar = {
            TopAppBar(
                title = { Text("SynesthesiaVibe", style = MaterialTheme.typography.titleLarge) },
                actions = {
                    IconButton(onClick = { controller.show(SampleScreen.Playlist) }) {
                        Icon(SampleIcons.QueueMusic, contentDescription = "Open playlist", tint = SvColors.TextSecondary)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = SvColors.BackgroundPrimary,
                    titleContentColor = SvColors.TextPrimary,
                ),
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState()),
        ) {
            VisualizerSurface(controller = controller, fillScreen = false)
            NowPlayingBar(controller)
            ListItem(
                headlineContent = { Text("Playlist", color = SvColors.TextPrimary) },
                supportingContent = {
                    Text("${controller.folderName} · ${controller.playlist.size} presets", color = SvColors.TextMuted)
                },
                leadingContent = { Icon(SampleIcons.QueueMusic, contentDescription = null, tint = SvColors.TextSecondary) },
                trailingContent = { Icon(SampleIcons.ChevronRight, contentDescription = null, tint = SvColors.TextMuted) },
                colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                modifier = Modifier.clickable { controller.show(SampleScreen.Playlist) },
            )
            SectionDivider()
            SectionHeader("Auto-change")
            SwitchRow(
                title = "Auto-change presets",
                supporting = "Moves to the next preset in the playlist",
                checked = controller.autoChangeEnabled,
                onCheckedChange = controller::setAutoChange,
            )
            DropdownRow(
                label = "Change every",
                value = "$AUTO_CHANGE_SECONDS seconds",
                options = listOf("$AUTO_CHANGE_SECONDS seconds"),
                enabled = controller.autoChangeEnabled,
                onSelected = {},
            )
            SectionDivider()
            SectionHeader("Sound source")
            SoundSourceRows(
                selected = controller.audioSource,
                showsSystemAudio = controller.showsSystemAudio,
                onSourceSelected = onSourceSelected,
            )
            SectionDivider()
            SectionHeader("Playback")
            FrameRateRow(controller)
            Spacer(Modifier.height(20.dp))
            TransitionRow()
            Spacer(Modifier.height(24.dp))
            SectionDivider()
            SectionHeader("Developer")
            SwitchRow(
                title = "Debug overlay",
                supporting = "FPS, frame time, audio levels and preset info, in full screen too",
                checked = controller.debugEnabled,
                onCheckedChange = controller::applyDebugEnabled,
            )
            Spacer(Modifier.height(24.dp))
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun PlaylistScreen(
    controller: VisualizerController,
    onPickFolder: () -> Unit,
) {
    val snackbarHostState = errorSnackbarState(controller)
    Scaffold(
        containerColor = SvColors.BackgroundPrimary,
        snackbarHost = { ErrorSnackbarHost(snackbarHostState) },
        topBar = {
            TopAppBar(
                navigationIcon = {
                    IconButton(onClick = controller::back) {
                        Icon(SampleIcons.ArrowBack, contentDescription = "Back", tint = SvColors.TextPrimary)
                    }
                },
                title = { Text("Playlist", style = MaterialTheme.typography.titleLarge) },
                actions = {
                    IconButton(onClick = onPickFolder, enabled = !controller.importing) {
                        Icon(SampleIcons.CreateNewFolder, contentDescription = "Choose presets folder", tint = SvColors.TextSecondary)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = SvColors.BackgroundPrimary,
                    titleContentColor = SvColors.TextPrimary,
                ),
            )
        },
    ) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding)) {
            ListItem(
                headlineContent = { Text(controller.folderName, color = SvColors.TextPrimary) },
                supportingContent = {
                    val imported = controller.importedFiles
                    val status = if (imported == null) "${controller.playlist.size} presets · Tap to change folder" else "Importing folder · $imported files copied"
                    Text(status, color = SvColors.TextMuted)
                },
                leadingContent = { Icon(SampleIcons.Folder, contentDescription = null, tint = SvColors.Info) },
                colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                modifier = Modifier.clickable(enabled = !controller.importing, onClick = onPickFolder),
            )
            if (controller.importing) {
                LinearProgressIndicator(
                    color = SvColors.AccentPrimary,
                    trackColor = SvColors.BorderSubtle,
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
                )
            }
            HorizontalDivider(color = SvColors.BorderSubtle, modifier = Modifier.padding(horizontal = 16.dp))
            LazyColumn(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                contentPadding = PaddingValues(vertical = 8.dp),
            ) {
                itemsIndexed(controller.playlist, key = { _, preset -> preset.file.path }) { index, preset ->
                    PresetRow(
                        number = index + 1,
                        preset = preset,
                        isCurrent = preset.file == controller.preset.file,
                        onClick = { controller.selectPreset(preset) },
                    )
                }
            }
            MiniPlayer(controller)
        }
    }
}

@Composable
private fun PresetRow(number: Int, preset: Preset, isCurrent: Boolean, onClick: () -> Unit) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        modifier = Modifier
            .padding(horizontal = 12.dp, vertical = 1.dp)
            .fillMaxWidth()
            .background(if (isCurrent) SvColors.AccentSubtle else Color.Transparent, RoundedCornerShape(28.dp))
            .clickable(onClick = onClick)
            .padding(start = 4.dp, end = 16.dp, top = 16.dp, bottom = 16.dp),
    ) {
        Text(
            text = number.toString(),
            color = if (isCurrent) SvColors.AccentText else SvColors.TextMuted,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.width(24.dp),
        )
        Text(
            text = preset.name,
            color = SvColors.TextPrimary,
            fontWeight = if (isCurrent) FontWeight.Medium else FontWeight.Normal,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
        if (isCurrent) {
            Icon(SampleIcons.GraphicEq, contentDescription = "Current preset", tint = SvColors.AccentText)
        }
    }
}

@Composable
private fun MiniPlayer(controller: VisualizerController) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        modifier = Modifier
            .fillMaxWidth()
            .background(SvColors.SurfaceCard)
            .padding(start = 16.dp, end = 16.dp, top = 12.dp, bottom = 16.dp),
    ) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .background(SvColors.AccentSubtle, RoundedCornerShape(12.dp)),
            contentAlignment = Alignment.Center,
        ) {
            Icon(SampleIcons.GraphicEq, contentDescription = null, tint = SvColors.AccentText)
        }
        Column(
            modifier = Modifier.weight(1f).clickable { controller.back() },
        ) {
            Text(
                controller.preset.name,
                color = SvColors.TextPrimary,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text("Now playing · ${nowPlayingSubtitle(controller)}", color = SvColors.TextMuted, style = MaterialTheme.typography.bodyMedium)
        }
        PauseButton(controller, small = true)
        IconButton(onClick = controller::stepNext) {
            Icon(SampleIcons.SkipNext, contentDescription = "Next preset", tint = SvColors.TextPrimary)
        }
    }
}

@Composable
private fun NowPlayingBar(controller: VisualizerController) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        modifier = Modifier
            .fillMaxWidth()
            .background(SvColors.SurfaceCard)
            .padding(horizontal = 16.dp, vertical = 12.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                controller.preset.name,
                color = SvColors.TextPrimary,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(nowPlayingSubtitle(controller), color = SvColors.TextMuted, style = MaterialTheme.typography.bodyMedium)
        }
        FilledTonalIconButton(
            onClick = controller::stepPrevious,
            colors = IconButtonDefaults.filledTonalIconButtonColors(
                containerColor = SvColors.SurfaceElevated,
                contentColor = SvColors.TextPrimary,
            ),
        ) {
            Icon(SampleIcons.SkipPrevious, contentDescription = "Previous preset")
        }
        PauseButton(controller, small = false)
        FilledTonalIconButton(
            onClick = controller::stepNext,
            colors = IconButtonDefaults.filledTonalIconButtonColors(
                containerColor = SvColors.SurfaceElevated,
                contentColor = SvColors.TextPrimary,
            ),
        ) {
            Icon(SampleIcons.SkipNext, contentDescription = "Next preset")
        }
    }
}

@Composable
private fun PauseButton(controller: VisualizerController, small: Boolean) {
    val size = if (small) 48.dp else 56.dp
    FloatingActionButton(
        onClick = { controller.applyPlaying(!controller.playing) },
        shape = RoundedCornerShape(16.dp),
        containerColor = SvColors.AccentPrimary,
        contentColor = SvColors.TextOnAccent,
        modifier = Modifier.size(size),
    ) {
        Icon(
            if (controller.playing) SampleIcons.Pause else SampleIcons.PlayArrow,
            contentDescription = if (controller.playing) "Pause" else "Play",
        )
    }
}

private fun nowPlayingSubtitle(controller: VisualizerController): String {
    val index = controller.playlist.indexOfFirst { it.file == controller.preset.file }
    val position = if (index < 0) 1 else index + 1
    val auto = if (controller.autoChangeEnabled) {
        "Next preset in ${controller.secondsUntilChange} s"
    } else {
        "Auto-change off"
    }
    return "$position of ${controller.playlist.size} · $auto"
}

@Composable
private fun SectionHeader(text: String) {
    Text(
        text = text,
        color = SvColors.AccentText,
        style = MaterialTheme.typography.titleSmall,
        fontWeight = FontWeight.Medium,
        modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 16.dp),
    )
}

@Composable
private fun SectionDivider() {
    HorizontalDivider(color = SvColors.BorderSubtle, modifier = Modifier.padding(top = 16.dp, start = 16.dp, end = 16.dp))
}

@Composable
private fun SwitchRow(
    title: String,
    supporting: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp, end = 24.dp, top = 8.dp, bottom = 8.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(title, color = SvColors.TextPrimary)
            Text(supporting, color = SvColors.TextMuted, style = MaterialTheme.typography.bodyMedium)
        }
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedTrackColor = SvColors.AccentPrimary,
                checkedThumbColor = SvColors.TextOnAccent,
                checkedIconColor = SvColors.AccentPrimary,
                uncheckedTrackColor = SvColors.SurfaceElevated,
                uncheckedBorderColor = SvColors.BorderStrong,
                uncheckedThumbColor = SvColors.BorderStrong,
            ),
            thumbContent = if (checked) {
                { Icon(SampleIcons.Check, contentDescription = null, tint = SvColors.AccentPrimary) }
            } else {
                null
            },
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DropdownRow(
    label: String,
    value: String,
    options: List<String>,
    enabled: Boolean,
    onSelected: (String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(
        expanded = expanded && enabled,
        onExpandedChange = { if (enabled) expanded = it },
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp, end = 16.dp, top = 8.dp)
            .alpha(if (enabled) 1f else DISABLED_ALPHA),
    ) {
        OutlinedTextField(
            value = value,
            onValueChange = {},
            readOnly = true,
            enabled = enabled,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded && enabled) },
            modifier = Modifier
                .fillMaxWidth()
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable),
        )
        ExposedDropdownMenu(expanded = expanded && enabled, onDismissRequest = { expanded = false }) {
            options.forEach { option ->
                DropdownMenuItem(
                    text = { Text(option) },
                    onClick = {
                        onSelected(option)
                        expanded = false
                    },
                )
            }
        }
    }
}

@Composable
private fun SoundSourceRows(
    selected: AudioSource,
    showsSystemAudio: Boolean,
    onSourceSelected: (AudioSource) -> Unit,
) {
    Column(modifier = Modifier.selectableGroup()) {
        AudioSource.entries.forEach { source ->
            val enabled = source != AudioSource.SystemAudio || showsSystemAudio
            val note = when (source) {
                AudioSource.None -> "Presets animate without audio"
                AudioSource.Wav -> "Sample track in the app"
                AudioSource.Microphone -> "Asks for RECORD_AUDIO"
                AudioSource.SystemAudio ->
                    if (showsSystemAudio) "Asks to capture your screen audio" else "Needs Android 10 or later"
            }
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .alpha(if (enabled) 1f else DISABLED_ALPHA)
                    .selectable(
                        selected = enabled && source == selected,
                        enabled = enabled,
                        onClick = { onSourceSelected(source) },
                    )
                    .padding(start = 16.dp, end = 24.dp, top = 12.dp, bottom = 12.dp),
            ) {
                RadioButton(
                    selected = source == selected,
                    onClick = null,
                    enabled = enabled,
                    colors = RadioButtonDefaults.colors(
                        selectedColor = SvColors.AccentText,
                        unselectedColor = SvColors.TextSecondary,
                    ),
                )
                Column(modifier = Modifier.weight(1f)) {
                    Text(source.label, color = SvColors.TextPrimary)
                    Text(note, color = SvColors.TextMuted, style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}

@Composable
private fun FrameRateRow(controller: VisualizerController) {
    Column(modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 12.dp)) {
        Text("Frame rate", color = SvColors.TextPrimary)
        Spacer(Modifier.height(8.dp))
        SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
            FpsOptions.forEachIndexed { index, option ->
                SegmentedButton(
                    selected = controller.fps == option.first,
                    onClick = { controller.applyFps(option.first) },
                    shape = SegmentedButtonDefaults.itemShape(index = index, count = FpsOptions.size),
                    colors = SegmentedButtonDefaults.colors(
                        activeContainerColor = SvColors.AccentSubtle,
                        activeContentColor = SvColors.TextPrimary,
                        activeBorderColor = SvColors.BorderStrong,
                        inactiveContainerColor = Color.Transparent,
                        inactiveContentColor = SvColors.TextPrimary,
                        inactiveBorderColor = SvColors.BorderStrong,
                        disabledActiveContainerColor = SvColors.SurfacePressed,
                        disabledActiveContentColor = SvColors.TextPrimary,
                        disabledActiveBorderColor = SvColors.BorderStrong,
                        disabledInactiveContainerColor = SvColors.SurfacePressed,
                        disabledInactiveContentColor = SvColors.TextPrimary,
                        disabledInactiveBorderColor = SvColors.BorderStrong,
                    ),
                    label = { Text(option.second) },
                )
            }
        }
        Spacer(Modifier.height(8.dp))
        Text("Changing it restarts the visualizer.", color = SvColors.TextMuted, style = MaterialTheme.typography.bodySmall)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TransitionRow() {
    var expanded by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp, end = 16.dp, top = 20.dp),
    ) {
        OutlinedTextField(
            value = "Blend",
            onValueChange = {},
            readOnly = true,
            label = { Text("Transition") },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
            modifier = Modifier
                .fillMaxWidth()
                .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            DropdownMenuItem(text = { Text("Blend") }, onClick = { expanded = false })
        }
    }
}

@Composable
private fun FullscreenScreen(controller: VisualizerController) {
    ImmersiveMode()
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(SvColors.Letterbox),
    ) {
        VisualizerSurface(controller = controller, fillScreen = true)
        Box(modifier = Modifier.align(Alignment.TopEnd).padding(top = 40.dp, end = 68.dp)) {
            BugButton(controller)
        }
        Box(modifier = Modifier.align(Alignment.TopEnd).padding(top = 40.dp, end = 12.dp)) {
            FilledTonalIconButton(
                onClick = { controller.show(SampleScreen.Visualizer) },
                colors = IconButtonDefaults.filledTonalIconButtonColors(
                    containerColor = SvColors.Overlay,
                    contentColor = SvColors.TextOnAccent,
                ),
            ) {
                Icon(SampleIcons.FullscreenExit, contentDescription = "Exit full screen")
            }
        }
    }
}

@Composable
private fun ImmersiveMode() {
    val view = LocalView.current
    DisposableEffect(view) {
        val activity = view.context as? Activity
        val window = activity?.window
        val controller = window?.let { WindowCompat.getInsetsController(it, view) }
        controller?.hide(WindowInsetsCompat.Type.systemBars())
        controller?.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
        onDispose {
            controller?.show(WindowInsetsCompat.Type.systemBars())
            activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        }
    }
}

@Composable
private fun BugButton(controller: VisualizerController) {
    FilledIconToggleButton(
        checked = controller.debugEnabled,
        onCheckedChange = controller::applyDebugEnabled,
        colors = IconButtonDefaults.filledIconToggleButtonColors(
            containerColor = SvColors.Overlay,
            contentColor = SvColors.TextOnAccent,
            checkedContainerColor = SvColors.AccentPrimary,
            checkedContentColor = SvColors.TextOnAccent,
        ),
        modifier = Modifier.border(1.dp, SvColors.OverlayHairline, CircleShape),
    ) {
        Icon(
            SampleIcons.BugReport,
            contentDescription = "Debug overlay",
            modifier = Modifier.size(20.dp),
        )
    }
}

@Composable
private fun VisualizerSurface(controller: VisualizerController, fillScreen: Boolean) {
    Box(
        modifier = if (fillScreen) Modifier.fillMaxSize() else Modifier.fillMaxWidth().aspectRatio(1f),
    ) {
        key(controller.fps) {
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { contextValue -> GLSurfaceView(contextValue).also(controller::attach) },
            )
        }
        if (!fillScreen) {
            Box(modifier = Modifier.align(Alignment.TopEnd).padding(top = 4.dp, end = 52.dp)) {
                BugButton(controller)
            }
            Box(modifier = Modifier.align(Alignment.TopEnd).padding(top = 4.dp, end = 4.dp)) {
                FilledTonalIconButton(
                    onClick = { controller.show(SampleScreen.Fullscreen) },
                    colors = IconButtonDefaults.filledTonalIconButtonColors(
                        containerColor = SvColors.Overlay,
                        contentColor = SvColors.TextOnAccent,
                    ),
                ) {
                    Icon(SampleIcons.Fullscreen, contentDescription = "Enter full screen")
                }
            }
        }
        if (controller.debugEnabled) {
            DebugCard(
                controller,
                controller.debugInfo,
                Modifier
                    .align(Alignment.TopStart)
                    .padding(top = if (fillScreen) 100.dp else 12.dp, start = 16.dp),
            )
        }
    }
}

@Composable
private fun DebugCard(controller: VisualizerController, info: Map<String, Any?>?, modifier: Modifier = Modifier) {
    if (info == null) return
    val displayFps = (info["display_fps"] as? Number)?.toFloat() ?: 0f
    val cap = if (controller.fps == VisualizerController.FPS_UNCAPPED) "none" else controller.fps.toInt().toString()
    val lastError = info["last_error"] as? String
    Column(
        modifier = modifier
            .width(220.dp)
            .background(SvColors.OverlayOpaque, RoundedCornerShape(8.dp))
            .border(1.dp, SvColors.OverlayHairline, RoundedCornerShape(8.dp))
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(
                "%.1f".format(displayFps),
                color = if (displayFps >= 50f) SvColors.Success else SvColors.Warning,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.SemiBold,
            )
            Text("fps · cap $cap", color = SvColors.TextMuted, style = monoSmall)
            Spacer(Modifier.weight(1f))
            Text("%.1f ms".format(if (displayFps > 0f) 1000f / displayFps else 0f), color = SvColors.TextMuted, style = monoSmall)
        }
        DebugGridRow("preset", (info["preset_name"] as? String).orEmpty())
        DebugGridRow("size", "${info["width"]}×${info["height"]} @1.0")
        DebugGridRow("gl", controller.glVersion)
        DebugGridRow("audio", audioDescription(controller))
        DebugGridRow("rate", "1.0× · Blend")
        DebugGridRow(
            "auto",
            if (controller.autoChangeEnabled) {
                "every $AUTO_CHANGE_SECONDS s · next ${controller.secondsUntilChange} s"
            } else {
                "off"
            },
        )
        DebugBar("bass", (info["bass"] as? Number)?.toFloat() ?: 0f, SvColors.AccentText)
        DebugBar("mid", (info["mid"] as? Number)?.toFloat() ?: 0f, SvColors.Info)
        DebugBar("treb", (info["treb"] as? Number)?.toFloat() ?: 0f, SvColors.Success)
        if (!lastError.isNullOrEmpty()) {
            Text(lastError, color = SvColors.Danger, style = monoSmall)
        }
    }
}

private fun audioDescription(controller: VisualizerController): String {
    val rate = controller.audioRate ?: return controller.audioSource.label
    return "${controller.audioSource.label} · ${"%.1f".format(rate / 1000f)} kHz"
}

@Composable
private fun DebugGridRow(label: String, value: String) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(label, color = SvColors.TextMuted, style = monoSmall, modifier = Modifier.width(52.dp))
        Text(value, color = SvColors.TextPrimary, style = monoSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun DebugBar(label: String, value: Float, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(label, color = SvColors.TextMuted, style = monoSmall, modifier = Modifier.width(34.dp))
        LinearProgressIndicator(
            progress = { (value / 3f).coerceIn(0f, 1f) },
            color = color,
            trackColor = SvColors.OverlayHairline,
            modifier = Modifier.weight(1f).height(6.dp),
        )
    }
}

private val monoSmall get() = androidx.compose.ui.text.TextStyle(
    fontFamily = FontFamily.Monospace,
    fontSize = 11.sp,
    lineHeight = 16.sp,
)

private val AudioSource.label: String
    get() = when (this) {
        AudioSource.None -> "None"
        AudioSource.Wav -> "Bundled WAV"
        AudioSource.Microphone -> "Microphone"
        AudioSource.SystemAudio -> "System sound"
    }
