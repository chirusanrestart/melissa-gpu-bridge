package dev.melissa.gpubridge

import android.app.Activity
import android.os.Bundle
import android.graphics.Typeface
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private external fun nativeGpuInfo(): String
    private external fun nativeComputeTest(): String

    companion object {
        init { System.loadLibrary("mgpu") }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        render()
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
            text = "Diagnóstico nativo • Vulkan Compute"
            textSize = 14f
            setTextColor(0xFFBDB6C8.toInt())
            setPadding(0, 6, 0, 24)
        }
        val info = TextView(this).apply {
            text = try { nativeGpuInfo() } catch (error: Throwable) {
                "Falha ao consultar Vulkan:\n\${error.message ?: error.javaClass.simpleName}"
            }
            textSize = 15f
            setTextColor(0xFFF1EDF5.toInt())
            typeface = Typeface.MONOSPACE
            setPadding(16, 18, 16, 18)
            setBackgroundColor(0xFF24202D.toInt())
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
                isEnabled = false
                computeStatus.text = "Executando shader na GPU..."
                Thread {
                    val result = try {
                        nativeComputeTest()
                    } catch (error: Throwable) {
                        "Falha no teste nativo:\n\${error.message ?: error.javaClass.simpleName}"
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
        root.addView(info)
        root.addView(computeButton)
        root.addView(computeStatus)
        setContentView(ScrollView(this).apply { addView(root) })
    }
}
