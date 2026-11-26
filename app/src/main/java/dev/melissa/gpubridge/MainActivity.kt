package dev.melissa.gpubridge

import android.app.Activity
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.ParcelFileDescriptor
import android.graphics.Typeface
import android.provider.OpenableColumns
import android.text.InputType
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import org.json.JSONObject
import java.io.File
import java.util.Locale

class MainActivity : Activity() {
    private external fun nativeGpuInfo(): String
    private external fun nativeComputeTest(elementCount: Int): String
    private external fun nativeFFmpegVulkanInfo(): String
    private external fun nativeVideoResize(inputFd: Int, outputPath: String, targetHeight: Int): String
    private external fun nativeVideoProgress(): String

    companion object {
        private const val MAX_ELEMENTS = 1_048_576
        private const val PICK_VIDEO_REQUEST = 4102
        init { System.loadLibrary("mgpu") }
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private var selectedVideo: Uri? = null
    private var selectedVideoName = "Nenhum vídeo selecionado"
    private var videoRunning = false
    private lateinit var videoStatus: TextView
    private lateinit var videoProgress: ProgressBar
    private lateinit var videoMetrics: TextView
    private lateinit var videoStartButton: Button
    private lateinit var resolutionSpinner: Spinner

    private val progressPoll = object : Runnable {
        override fun run() {
            if (!videoRunning) return
            try {
                val data = JSONObject(nativeVideoProgress())
                val percent = data.optInt("progress", 0).coerceIn(0, 100)
                videoProgress.progress = percent
                val frames = data.optLong("frames", 0)
                val total = data.optLong("totalFrames", 0)
                val fps = data.optDouble("fps", 0.0)
                val speed = data.optDouble("speed", 0.0)
                val elapsed = data.optDouble("elapsed", 0.0)
                val eta = data.optDouble("eta", 0.0)
                val totalLabel = if (total > 0) total.toString() else "?"
                videoMetrics.text = String.format(
                    Locale.US,
                    "Progresso: %d%%\nFrames: %d / %s\nFPS: %.2f\nVelocidade: %.2fx\nDecorrido: %s\nRestante: %s",
                    percent, frames, totalLabel, fps, speed, formatDuration(elapsed), formatDuration(eta)
                )
            } catch (_: Throwable) {
                // A próxima leitura atualiza os indicadores quando o estado nativo estiver disponível.
            }
            mainHandler.postDelayed(this, 500)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        render()
    }

    override fun onDestroy() {
        mainHandler.removeCallbacks(progressPoll)
        super.onDestroy()
    }

    @Deprecated("Deprecated in Android API Activity, retained for minSdk compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == PICK_VIDEO_REQUEST && resultCode == RESULT_OK) {
            val uri = data?.data ?: return
            selectedVideo = uri
            selectedVideoName = queryDisplayName(uri) ?: uri.lastPathSegment ?: "Vídeo selecionado"
            videoStatus.text = "Vídeo selecionado:\n" + selectedVideoName
            videoStartButton.isEnabled = true
        }
    }

    private fun queryDisplayName(uri: Uri): String? {
        var cursor: Cursor? = null
        return try {
            cursor = contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            if (cursor != null && cursor.moveToFirst()) {
                cursor.getString(cursor.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME))
            } else null
        } catch (_: Throwable) {
            null
        } finally {
            cursor?.close()
        }
    }

    private fun formatDuration(seconds: Double): String {
        if (!seconds.isFinite() || seconds < 0.0) return "--:--"
        val total = seconds.toLong()
        return String.format(Locale.US, "%02d:%02d", total / 60, total % 60)
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
                computeStatus.text = "Executando shader na GPU com " + count + " elementos..."
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

        val benchmarkTitle = TextView(this).apply {
            text = "Video Benchmark • Vulkan GPU"
            textSize = 19f
            typeface = Typeface.DEFAULT_BOLD
            setTextColor(0xFFFFB6DC.toInt())
            setPadding(0, 28, 0, 8)
        }
        val benchmarkDescription = TextView(this).apply {
            text = "Redimensiona um vídeo usando FFmpeg + scale_vulkan. A decodificação e a codificação são feitas pelo FFmpeg; o redimensionamento é executado no backend Vulkan e o áudio é copiado sem recodificação."
            textSize = 13f
            setTextColor(0xFFBDB6C8.toInt())
            setPadding(0, 0, 0, 12)
        }
        val pickVideoButton = Button(this).apply {
            text = "Selecionar vídeo da galeria"
            isAllCaps = false
            setOnClickListener {
                val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = "video/*"
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                startActivityForResult(intent, PICK_VIDEO_REQUEST)
            }
        }
        videoStatus = TextView(this).apply {
            text = selectedVideoName
            textSize = 13f
            setTextColor(0xFFF1EDF5.toInt())
            setPadding(12, 12, 12, 12)
            setBackgroundColor(0xFF24202D.toInt())
        }
        val resolutionLabel = TextView(this).apply {
            text = "Resolução de saída"
            textSize = 14f
            setTextColor(0xFFF1EDF5.toInt())
            setPadding(0, 12, 0, 4)
        }
        resolutionSpinner = Spinner(this).apply {
            adapter = ArrayAdapter(
                this@MainActivity,
                android.R.layout.simple_spinner_dropdown_item,
                listOf("360p", "480p", "720p", "1080p")
            )
            setSelection(1)
        }
        videoStartButton = Button(this).apply {
            text = "Iniciar benchmark Vulkan GPU"
            isAllCaps = false
            isEnabled = false
            setOnClickListener { startVideoBenchmark() }
        }
        videoProgress = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 100
            progress = 0
            visibility = View.VISIBLE
        }
        videoMetrics = TextView(this).apply {
            text = "Progresso: aguardando vídeo\nFPS: --\nVelocidade: --\nTempo: --"
            textSize = 13f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(12, 12, 12, 12)
            setBackgroundColor(0xFF24202D.toInt())
        }

        root.addView(title)
        root.addView(subtitle)
        root.addView(info)
        root.addView(ffmpegButton)
        root.addView(ffmpegStatus)
        root.addView(countLabel)
        root.addView(countInput)
        root.addView(presets)
        root.addView(computeButton)
        root.addView(computeStatus)
        root.addView(benchmarkTitle)
        root.addView(benchmarkDescription)
        root.addView(pickVideoButton)
        root.addView(videoStatus)
        root.addView(resolutionLabel)
        root.addView(resolutionSpinner)
        root.addView(videoStartButton)
        root.addView(videoProgress)
        root.addView(videoMetrics)
        setContentView(ScrollView(this).apply { addView(root) })
    }

    private fun startVideoBenchmark() {
        val uri = selectedVideo
        if (uri == null || videoRunning) return

        val targetHeight = when (resolutionSpinner.selectedItemPosition) {
            0 -> 360
            1 -> 480
            2 -> 720
            else -> 1080
        }
        val outputDirectory = getExternalFilesDir(Environment.DIRECTORY_MOVIES) ?: cacheDir
        if (!outputDirectory.exists() && !outputDirectory.mkdirs()) {
            videoStatus.text = "Não foi possível criar a pasta de saída."
            return
        }
        val outputFile = File(outputDirectory, "mgpu_resize_" + System.currentTimeMillis() + ".mp4")

        videoRunning = true
        videoStartButton.isEnabled = false
        videoProgress.progress = 0
        videoStatus.text = "Processando: " + selectedVideoName + "\nSaída: " + outputFile.absolutePath
        videoMetrics.text = "Preparando FFmpeg + Vulkan..."
        mainHandler.removeCallbacks(progressPoll)
        mainHandler.post(progressPoll)

        Thread {
            var descriptor: ParcelFileDescriptor? = null
            val result = try {
                descriptor = contentResolver.openFileDescriptor(uri, "r")
                    ?: throw IllegalStateException("O Android não conseguiu abrir o vídeo selecionado.")
                nativeVideoResize(descriptor.fd, outputFile.absolutePath, targetHeight)
            } catch (error: Throwable) {
                "VIDEO BENCHMARK: FALHOU\n" + (error.message ?: error.javaClass.simpleName)
            } finally {
                try { descriptor?.close() } catch (_: Throwable) {}
            }

            runOnUiThread {
                videoRunning = false
                mainHandler.removeCallbacks(progressPoll)
                videoStartButton.isEnabled = selectedVideo != null
                videoStatus.text = result + "\n\nArquivo de saída:\n" + outputFile.absolutePath
                if (result.startsWith("VIDEO BENCHMARK: CONCLUÍDO")) {
                    videoProgress.progress = 100
                }
                try {
                    val data = JSONObject(nativeVideoProgress())
                    videoMetrics.text = String.format(
                        Locale.US,
                        "Progresso: %d%%\nFrames: %d / %d\nFPS médio: %.2f\nVelocidade: %.2fx\nTempo decorrido: %s",
                        data.optInt("progress", 0),
                        data.optLong("frames", 0),
                        data.optLong("totalFrames", 0),
                        data.optDouble("fps", 0.0),
                        data.optDouble("speed", 0.0),
                        formatDuration(data.optDouble("elapsed", 0.0))
                    )
                } catch (_: Throwable) {
                    videoMetrics.text = "Benchmark finalizado."
                }
            }
        }.start()
    }
}
