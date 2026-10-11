//
//  ProjectionService.kt
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

package com.synesthesiavibe.sample

import android.app.Activity
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class ProjectionService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION)
        } else {
            startForeground(NOTIFICATION_ID, notification())
        }
        val resultCode = intent?.getIntExtra(EXTRA_RESULT_CODE, Activity.RESULT_CANCELED) ?: Activity.RESULT_CANCELED
        val data = intent?.let(::consentData)
        if (resultCode != Activity.RESULT_OK || data == null) {
            stopSelf()
            return START_NOT_STICKY
        }

        val projection = getSystemService(MediaProjectionManager::class.java).getMediaProjection(resultCode, data)
        if (projection == null) {
            stopSelf()
            return START_NOT_STICKY
        }
        projection.registerCallback(object : MediaProjection.Callback() {
            override fun onStop() {
                Log.i(TAG, "MediaProjection stopped")
                current.value = null
                stopSelf()
            }
        }, Handler(Looper.getMainLooper()))
        Log.i(TAG, "MediaProjection granted")
        current.value = projection
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        current.value?.stop()
        current.value = null
        super.onDestroy()
    }

    private fun notification(): Notification {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel(CHANNEL_ID, getString(R.string.projection_channel), NotificationManager.IMPORTANCE_LOW))
        return Notification.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(getString(R.string.projection_notification))
            .setOngoing(true)
            .build()
    }

    private fun consentData(intent: Intent): Intent? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(EXTRA_DATA, Intent::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(EXTRA_DATA)
        }

    companion object {

        private const val TAG = "SynesthesiaVibeSample"
        private const val CHANNEL_ID = "system-audio"
        private const val NOTIFICATION_ID = 1
        private const val EXTRA_RESULT_CODE = "result-code"
        private const val EXTRA_DATA = "data"

        private val current = MutableStateFlow<MediaProjection?>(null)
        val projection: StateFlow<MediaProjection?> = current.asStateFlow()

        fun start(context: Context, resultCode: Int, data: Intent) {
            val intent = Intent(context, ProjectionService::class.java)
                .putExtra(EXTRA_RESULT_CODE, resultCode)
                .putExtra(EXTRA_DATA, data)
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, ProjectionService::class.java))
        }
    }
}
