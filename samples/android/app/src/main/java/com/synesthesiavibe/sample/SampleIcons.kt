//
//  SampleIcons.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

package com.synesthesiavibe.sample

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathBuilder
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp

object SampleIcons {

    val SkipPrevious: ImageVector by lazy {
        icon("SkipPrevious") {
            moveTo(18f, 6f); lineTo(10f, 12f); lineTo(18f, 18f); close()
            moveTo(6f, 6f); lineTo(6f, 18f)
        }
    }

    val SkipNext: ImageVector by lazy {
        icon("SkipNext") {
            moveTo(6f, 6f); lineTo(14f, 12f); lineTo(6f, 18f); close()
            moveTo(18f, 6f); lineTo(18f, 18f)
        }
    }

    val PlayArrow: ImageVector by lazy {
        icon("PlayArrow") {
            moveTo(7f, 4f); lineTo(20f, 12f); lineTo(7f, 20f); close()
        }
    }

    val Pause: ImageVector by lazy {
        icon("Pause") {
            moveTo(8f, 5f); lineTo(8f, 19f)
            moveTo(16f, 5f); lineTo(16f, 19f)
        }
    }

    val CreateNewFolder: ImageVector by lazy {
        icon("CreateNewFolder") {
            moveTo(3f, 7f)
            arcToRelative(2f, 2f, 0f, false, true, 2f, -2f)
            horizontalLineToRelative(4f)
            lineToRelative(2f, 2f)
            horizontalLineToRelative(8f)
            arcToRelative(2f, 2f, 0f, false, true, 2f, 2f)
            verticalLineToRelative(8f)
            arcToRelative(2f, 2f, 0f, false, true, -2f, 2f)
            horizontalLineTo(5f)
            arcToRelative(2f, 2f, 0f, false, true, -2f, -2f)
            close()
            moveTo(12f, 11f); lineTo(12f, 16f)
            moveTo(9.5f, 13.5f); lineTo(14.5f, 13.5f)
        }
    }

    val Folder: ImageVector by lazy {
        icon("Folder") {
            moveTo(3f, 7f)
            arcToRelative(2f, 2f, 0f, false, true, 2f, -2f)
            horizontalLineToRelative(4f)
            lineToRelative(2f, 2f)
            horizontalLineToRelative(8f)
            arcToRelative(2f, 2f, 0f, false, true, 2f, 2f)
            verticalLineToRelative(8f)
            arcToRelative(2f, 2f, 0f, false, true, -2f, 2f)
            horizontalLineTo(5f)
            arcToRelative(2f, 2f, 0f, false, true, -2f, -2f)
            close()
        }
    }

    val Fullscreen: ImageVector by lazy {
        icon("Fullscreen") {
            moveTo(4f, 9f); verticalLineTo(4f); horizontalLineTo(9f)
            moveTo(20f, 9f); verticalLineTo(4f); horizontalLineTo(15f)
            moveTo(4f, 15f); verticalLineTo(20f); horizontalLineTo(9f)
            moveTo(20f, 15f); verticalLineTo(20f); horizontalLineTo(15f)
        }
    }

    val FullscreenExit: ImageVector by lazy {
        icon("FullscreenExit") {
            moveTo(9f, 4f); verticalLineTo(9f); horizontalLineTo(4f)
            moveTo(15f, 4f); verticalLineTo(9f); horizontalLineTo(20f)
            moveTo(9f, 20f); verticalLineTo(15f); horizontalLineTo(4f)
            moveTo(15f, 20f); verticalLineTo(15f); horizontalLineTo(20f)
        }
    }

    val BugReport: ImageVector by lazy {
        icon("BugReport") {
            moveTo(12f, 19f); curveToRelative(-2.2f, 0f, -4f, -1.8f, -4f, -4f)
            verticalLineTo(10f); curveToRelative(0f, -2.2f, 1.8f, -4f, 4f, -4f)
            curveToRelative(2.2f, 0f, 4f, 1.8f, 4f, 4f); verticalLineToRelative(5f)
            curveToRelative(0f, 2.2f, -1.8f, 4f, -4f, 4f); close()
            moveTo(12f, 11f); lineTo(12f, 19f)
            moveTo(4f, 13f); lineTo(8f, 13f)
            moveTo(16f, 13f); lineTo(20f, 13f)
            moveTo(5f, 7f); lineTo(8f, 9f)
            moveTo(19f, 7f); lineTo(16f, 9f)
            moveTo(5f, 19f); lineTo(8f, 17f)
            moveTo(19f, 19f); lineTo(16f, 17f)
        }
    }

    val QueueMusic: ImageVector by lazy {
        icon("QueueMusic") {
            moveTo(4f, 6f); lineTo(15f, 6f)
            moveTo(4f, 12f); lineTo(15f, 12f)
            moveTo(4f, 18f); lineTo(11f, 18f)
            moveTo(19f, 5f); lineTo(19f, 15f)
            circle(17f, 16f, 2f)
        }
    }

    val ArrowBack: ImageVector by lazy {
        icon("ArrowBack") {
            moveTo(19f, 12f); lineTo(5f, 12f)
            moveTo(11f, 6f); lineTo(5f, 12f); lineTo(11f, 18f)
        }
    }

    val Check: ImageVector by lazy {
        icon("Check") {
            moveTo(5f, 12.5f); lineTo(9.5f, 17f); lineTo(19f, 7f)
        }
    }

    val ExpandMore: ImageVector by lazy {
        icon("ExpandMore", fill = true) {
            moveTo(7f, 10f); lineTo(12f, 15f); lineTo(17f, 10f); close()
        }
    }

    val ChevronRight: ImageVector by lazy {
        icon("ChevronRight") {
            moveTo(9f, 6f); lineTo(15f, 12f); lineTo(9f, 18f)
        }
    }

    val GraphicEq: ImageVector by lazy {
        icon("GraphicEq") {
            moveTo(4f, 10f); lineTo(4f, 14f)
            moveTo(8f, 7f); lineTo(8f, 17f)
            moveTo(12f, 4f); lineTo(12f, 20f)
            moveTo(16f, 7f); lineTo(16f, 17f)
            moveTo(20f, 10f); lineTo(20f, 14f)
        }
    }

    private fun PathBuilder.circle(cx: Float, cy: Float, radius: Float) {
        moveTo(cx - radius, cy)
        arcTo(radius, radius, 0f, true, true, cx + radius, cy)
        arcTo(radius, radius, 0f, true, true, cx - radius, cy)
        close()
    }

    private fun icon(
        name: String,
        fill: Boolean = false,
        pathBuilder: PathBuilder.() -> Unit,
    ): ImageVector {
        val stroke = if (fill) null else SolidColor(Color.White)
        val brush = if (fill) SolidColor(Color.White) else null
        return ImageVector.Builder(
            name = name,
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).path(
            fill = brush,
            stroke = stroke,
            strokeLineWidth = 1.75f,
            strokeLineCap = androidx.compose.ui.graphics.StrokeCap.Round,
            strokeLineJoin = androidx.compose.ui.graphics.StrokeJoin.Round,
            pathBuilder = pathBuilder,
        ).build()
    }
}
