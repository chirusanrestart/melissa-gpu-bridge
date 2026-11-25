package dev.melissa.gpubridge

import android.app.Activity
import android.os.Bundle
import android.graphics.Typeface
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private external fun nativeGpuInfo(): String

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
            text = "Diagnóstico nativo • Vulkan"
            textSize = 14f
            setTextColor(0xFFBDB6C8.toInt())
            setPadding(0, 6, 0, 24)
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
        root.addView(info)
        val scroll = ScrollView(this).apply { addView(root) }
        setContentView(scroll)
    }
}
