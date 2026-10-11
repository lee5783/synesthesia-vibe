//
//  CaptureScreen.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.opengl.GLSurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.synesthesiavibe.SynesthesiaRenderer

@Composable
fun CaptureScreen(request: CaptureRequest, onFinished: (CaptureResult) -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black),
    ) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context ->
                GLSurfaceView(context).apply {
                    SynesthesiaRenderer.configure(this)
                    setRenderer(CaptureRenderer(request, onFinished))
                    renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
                }
            },
        )
        Text(
            text = "Capturing ${request.preset.name}\n${request.frames.size} frames",
            color = Color.White,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .align(Alignment.Center)
                .padding(32.dp),
        )
    }
}
