package dev.melissa.gpubridge

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.graphics.Typeface
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private external fun nativeGpuInfo(): String
    private lateinit var videoPermissionStatus: TextView

    companion object {
        private const val REQUEST_VIDEO_PERMISSION = 1001

        init { System.loadLibrary("mgpu") }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        render()
        checkVideoPermission()
    }

    private fun checkVideoPermission() {
        if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) ==
            PackageManager.PERMISSION_GRANTED
        ) {
            updateVideoPermissionStatus(true)
        } else {
            updateVideoPermissionStatus(false)
            requestPermissions(
                arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE),
                REQUEST_VIDEO_PERMISSION
            )
        }
    }

    private fun updateVideoPermissionStatus(granted: Boolean) {
        videoPermissionStatus.text = if (granted) {
            "Acesso aos vídeos: permitido"
        } else {
            "Acesso aos vídeos: não permitido\nToque aqui para solicitar novamente."
        }
    }

    @Deprecated("Deprecated in Android API, retained for compatibility with the app's minSdk")
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_VIDEO_PERMISSION) {
            updateVideoPermissionStatus(
                grantResults.isNotEmpty() &&
                    grantResults[0] == PackageManager.PERMISSION_GRANTED
            )
        }
    }

    private fun render() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(24, 32, 24, 24)
            setBackgroundColor(0xFF121018.toInt())
        }
        val title = TextView(this).apply {
            text = "Melissa GPU Bridge"
            textSize = 24f
            setTextColor(0xFFFFB6DC.toInt())
            typeface = Typeface.DEFAULT_BOLD
        }
        val subtitle = TextView(this).apply {
            text = "Diagnóstico nativo • Vulkan"
            textSize = 14f
            setTextColor(0xFFBDB6C8.toInt())
            setPadding(0, 6, 0, 24)
        }
        videoPermissionStatus = TextView(this).apply {
            textSize = 14f
            setTextColor(0xFFFFB6DC.toInt())
            setPadding(16, 14, 16, 14)
            setBackgroundColor(0xFF24202D.toInt())
            setOnClickListener {
                if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) !=
                    PackageManager.PERMISSION_GRANTED
                ) {
                    requestPermissions(
                        arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE),
                        REQUEST_VIDEO_PERMISSION
                    )
                }
            }
        }
        val info = TextView(this).apply {
            text = try { nativeGpuInfo() } catch (error: Throwable) {
                "Falha ao consultar Vulkan:\n${error.message ?: error.javaClass.simpleName}"
            }
            textSize = 15f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(16, 18, 16, 18)
            setBackgroundColor(0xFF24202D.toInt())
        }
        root.addView(title)
        root.addView(subtitle)
        root.addView(videoPermissionStatus)
        root.addView(info)
        val scroll = ScrollView(this).apply { addView(root) }
        setContentView(scroll)
    }
}
