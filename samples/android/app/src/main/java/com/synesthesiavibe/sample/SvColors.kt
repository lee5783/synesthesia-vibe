//
//  SvColors.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

package com.synesthesiavibe.sample

import androidx.compose.material3.darkColorScheme
import androidx.compose.ui.graphics.Color

object SvColors {

    val BackgroundPrimary = Color(0xFF1E1E1E)
    val BackgroundSecondary = Color(0xFF161616)
    val SurfaceCard = Color(0xFF262626)
    val SurfaceElevated = Color(0xFF2C2C2C)
    val SurfaceHover = Color(0xFF313131)
    val SurfacePressed = Color(0xFF383838)
    val BorderSubtle = Color(0xFF333333)
    val BorderStrong = Color(0xFF737373)
    val TextPrimary = Color(0xFFDCDDDE)
    val TextSecondary = Color(0xFFB4B4B6)
    val TextMuted = Color(0xFF949494)
    val TextDisabled = Color(0xFF5C5C5C)
    val TextOnAccent = Color(0xFFFFFFFF)
    val AccentPrimary = Color(0xFF7C3AED)
    val AccentHover = Color(0xFF6D28D9)
    val AccentPressed = Color(0xFF5B21B6)
    val AccentText = Color(0xFFA78BFA)
    val AccentSubtle = Color(0xFF2A2140)
    val Overlay = Color(0xA3000000)
    val OverlayOpaque = Color(0xD9000000)
    val OverlayHairline = Color(0x1FFFFFFF)
    val Success = Color(0xFF4ADE80)
    val Warning = Color(0xFFFBBF24)
    val Info = Color(0xFF60A5FA)
    val InfoSubtle = Color(0xFF16233A)
    val Danger = Color(0xFFF87171)
    val DangerSubtle = Color(0xFF2F1818)
    val Letterbox = Color(0xFF000000)

    val Scheme = darkColorScheme(
        primary = AccentText,
        onPrimary = Color(0xFF18181B),
        primaryContainer = AccentPrimary,
        onPrimaryContainer = TextOnAccent,
        secondaryContainer = AccentSubtle,
        onSecondaryContainer = TextPrimary,
        background = BackgroundPrimary,
        onBackground = TextPrimary,
        surface = BackgroundPrimary,
        onSurface = TextPrimary,
        surfaceVariant = SurfaceCard,
        onSurfaceVariant = TextSecondary,
        surfaceContainer = SurfaceCard,
        surfaceContainerHigh = SurfaceElevated,
        surfaceContainerHighest = SurfacePressed,
        outline = BorderStrong,
        outlineVariant = BorderSubtle,
        error = Danger,
        onError = TextOnAccent,
    )
}
