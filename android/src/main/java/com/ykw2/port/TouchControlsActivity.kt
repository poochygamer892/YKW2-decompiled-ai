package com.ykw2.port

import android.app.Activity
import android.content.res.Configuration
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import android.widget.ArrayAdapter

/** On-device editor for per-orientation virtual gamepad placement and scale. */
class TouchControlsActivity : Activity() {
    private lateinit var controls: ZakuroScreenView
    private lateinit var scaleBar: SeekBar
    private lateinit var targetSpinner: Spinner
    private lateinit var selectionLabel: TextView
    private var updatingSelection = false

    private val targets = listOf(
        "Todos", "Cruceta · arriba", "Cruceta · abajo", "Cruceta · izquierda", "Cruceta · derecha",
        "Joystick", "A", "B", "X", "Y", "Select", "Start", "L", "R",
    )
    private val targetKeys = listOf(
        TouchControlLayout.ALL, "UP", "DOWN", "LEFT", "RIGHT", "STICK", "A", "B", "X", "Y",
        "SELECT", "START", "L", "R",
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterImmersiveMode()
        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        controls = ZakuroScreenView(this) { _, _, _, _, _ -> }.apply { editorMode = true }
        root.addView(controls, FrameLayout.LayoutParams(-1, -1))

        val instructions = TextView(this).apply {
            text = "Toca un control para elegirlo o arrastra un grupo para moverlo"
            textSize = 13f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
        }
        selectionLabel = TextView(this).apply {
            text = "Tamaño · Todos"
            textSize = 12f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
        }
        targetSpinner = Spinner(this).apply {
            adapter = ArrayAdapter(this@TouchControlsActivity, android.R.layout.simple_spinner_dropdown_item, targets)
            setSelection(0, false)
            onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
                override fun onNothingSelected(parent: android.widget.AdapterView<*>?) = Unit
                override fun onItemSelected(parent: android.widget.AdapterView<*>?, view: View?, position: Int, id: Long) {
                    if (!updatingSelection && ::scaleBar.isInitialized && ::selectionLabel.isInitialized) {
                        updateSelectedControl(targetKeys[position])
                    }
                }
            }
        }
        val scale = SeekBar(this).apply {
            max = 150 // 0.50x to 2.00x
            progress = ((controls.selectedControlScale() - TouchControlLayout.MIN_SIZE) * 100).toInt().coerceIn(0, max)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                    controls.setSelectedControlScale(TouchControlLayout.MIN_SIZE + progress / 100f)
                }
                override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
            })
        }
        scaleBar = scale
        controls.onEditorSelectionChanged = { key ->
            val position = targetKeys.indexOf(key).takeIf { it >= 0 } ?: 0
            updatingSelection = true
            targetSpinner.setSelection(position)
            updatingSelection = false
            selectionLabel.text = "Tamaño · ${targets[position]}"
            scaleBar.progress = ((controls.selectedControlScale() - TouchControlLayout.MIN_SIZE) * 100)
                .toInt().coerceIn(0, scaleBar.max)
        }
        val actions = LinearLayout(this).apply { gravity = Gravity.CENTER }
        actions.addView(Button(this).apply {
            text = "Restablecer"
            setOnClickListener {
                controls.resetLayout()
                scale.progress = 50
            }
        })
        actions.addView(Button(this).apply {
            text = "Listo"
            setOnClickListener { finish() }
        })
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(12, 8, 12, 4)
            setBackgroundColor(0xdd111820.toInt())
            addView(instructions, LinearLayout.LayoutParams(-1, -2))
            addView(targetSpinner, LinearLayout.LayoutParams(-1, -2))
            addView(selectionLabel, LinearLayout.LayoutParams(-1, -2))
            addView(scale, LinearLayout.LayoutParams(-1, -2))
            addView(actions, LinearLayout.LayoutParams(-1, -2))
        }
        root.addView(panel, FrameLayout.LayoutParams(
            (resources.displayMetrics.density * 245).toInt().coerceAtMost(resources.displayMetrics.widthPixels),
            -2,
            Gravity.TOP or Gravity.CENTER_HORIZONTAL,
        ).apply { topMargin = (resources.displayMetrics.density * 8).toInt() })
        setContentView(root)
        controls.post {
            scale.progress = ((controls.selectedControlScale() - TouchControlLayout.MIN_SIZE) * 100)
                .toInt().coerceIn(0, scale.max)
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        enterImmersiveMode()
        controls.post {
            scaleBar.progress = ((controls.selectedControlScale() - TouchControlLayout.MIN_SIZE) * 100).toInt()
                .coerceIn(0, scaleBar.max)
        }
    }

    private fun updateSelectedControl(key: String) {
        controls.selectControl(key)
        val position = targetKeys.indexOf(key).coerceAtLeast(0)
        selectionLabel.text = "Tamaño · ${targets[position]}"
        scaleBar.progress = ((controls.selectedControlScale() - TouchControlLayout.MIN_SIZE) * 100)
            .toInt().coerceIn(0, scaleBar.max)
    }

    @Suppress("DEPRECATION")
    private fun enterImmersiveMode() {
        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            )
    }
}
