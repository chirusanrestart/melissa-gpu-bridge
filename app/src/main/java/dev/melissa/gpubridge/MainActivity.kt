package dev.melissa.gpubridge

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.graphics.Typeface
import android.text.InputType
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private external fun nativeGpuInfo(): String
    private external fun nativeComputeTest(elementCount: Int): String
    private external fun nativeFFmpegVulkanInfo(): String
    private lateinit var videoPermissionStatus: TextView

    companion object {
        private const val MAX_ELEMENTS = 1_048_576
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
            "Acesso aos vídeos: não permitido. Toque aqui para solicitar novamente."
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
            text = "Diagnóstico nativo • Vulkan Compute + FFmpeg"
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
                "Falha ao consultar Vulkan: " + (error.message ?: error.javaClass.simpleName)
            }
            textSize = 15f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(16, 18, 16, 18)
            setBackgroundColor(0xFF24202D.toInt())
        }
        val ffmpegStatus = TextView(this).apply {
            text = "FFmpeg Vulkan ainda não foi testado."
            textSize = 14f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(16, 18, 16, 18)
            setBackgroundColor(0xFF24202D.toInt())
        }
        val ffmpegButton = Button(this).apply {
            text = "Testar FFmpeg + Vulkan nativo"
            isAllCaps = false
            setOnClickListener {
                isEnabled = false
                ffmpegStatus.text = "FFmpeg está inicializando o dispositivo Vulkan..."
                Thread {
                    val result = try {
                        nativeFFmpegVulkanInfo()
                    } catch (error: Throwable) {
                        "Falha no FFmpeg nativo: " + (error.message ?: error.javaClass.simpleName)
                    }
                    runOnUiThread {
                        ffmpegStatus.text = result
                        isEnabled = true
                    }
                }.start()
            }
        }
        val countLabel = TextView(this).apply {
            text = "Quantidade de elementos (1 a 1.048.576)"
            textSize = 14f
            setTextColor(0xFFF1EDF5.toInt())
            setPadding(0, 20, 0, 6)
        }
        val countInput = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            setSingleLine(true)
            setText("256")
            textSize = 16f
            setTextColor(0xFFF1EDF5.toInt())
            setHintTextColor(0xFFBDB6C8.toInt())
            setPadding(14, 10, 14, 10)
            setBackgroundColor(0xFF24202D.toInt())
        }
        val presets = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 4, 0, 8)
        }
        val presetValues = listOf(
            "256" to 256,
            "4K" to 4_096,
            "64K" to 65_536,
            "256K" to 262_144,
            "1M" to MAX_ELEMENTS
        )
        presetValues.forEach { (label, value) ->
            val presetButton = Button(this).apply {
                text = label
                textSize = 10f
                isAllCaps = false
                minWidth = 0
                setPadding(2, 0, 2, 0)
                setOnClickListener {
                    countInput.setText(value.toString())
                    countInput.setSelection(countInput.text.length)
                }
            }
            presets.addView(
                presetButton,
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            )
        }
        val computeStatus = TextView(this).apply {
            text = "Teste Compute Shader ainda não executado."
            textSize = 14f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(16, 18, 16, 18)
            setBackgroundColor(0xFF24202D.toInt())
        }
        val computeButton = Button(this).apply {
            text = "Executar teste Vulkan Compute"
            isAllCaps = false
            setOnClickListener {
                val count = countInput.text.toString().toIntOrNull()
                if (count == null || count !in 1..MAX_ELEMENTS) {
                    computeStatus.text = "Quantidade inválida. Informe um valor entre 1 e 1.048.576."
                    return@setOnClickListener
                }

                isEnabled = false
                computeStatus.text = "Executando shader na GPU com $count elementos..."
                Thread {
                    val result = try {
                        nativeComputeTest(count)
                    } catch (error: Throwable) {
                        "Falha no teste nativo: " + (error.message ?: error.javaClass.simpleName)
                    }
                    runOnUiThread {
                        computeStatus.text = result
                        isEnabled = true
                    }
                }.start()
            }
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(videoPermissionStatus)
        root.addView(info)
        root.addView(ffmpegButton)
        root.addView(ffmpegStatus)
        root.addView(countLabel)
        root.addView(countInput)
        root.addView(presets)
        root.addView(computeButton)
        root.addView(computeStatus)
        setContentView(ScrollView(this).apply { addView(root) })
    }
}
