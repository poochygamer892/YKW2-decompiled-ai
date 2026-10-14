package com.ykw2.port

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView

/** Minimal native host for the portable RuntimeScene snapshot. */
class RuntimeMenuActivity : Activity() {
    private val frameHandler = Handler(Looper.getMainLooper())
    private val frameTicker = object : Runnable {
        override fun run() {
            if (isFinishing) return
            render(RomImportBridge.nativeRuntimeAdvance(1.0 / 60.0))
            frameHandler.postDelayed(this, 16L)
        }
    }
    private lateinit var selectedLabel: TextView
    private lateinit var status: TextView
    private lateinit var commandStatus: TextView
    private lateinit var backgroundImage: ImageView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val padding = (resources.displayMetrics.density * 24).toInt()
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(padding, padding, padding, padding)
        }
        layout.addView(TextView(this).apply {
            text = "Yo-kai Watch 2"
            textSize = 26f
            gravity = Gravity.CENTER
        })
        status = TextView(this).apply {
            setPadding(0, padding / 2, 0, padding / 2)
        }
        layout.addView(status)
        backgroundImage = ImageView(this).apply {
            adjustViewBounds = true
            visibility = View.GONE
        }
        layout.addView(backgroundImage)
        selectedLabel = TextView(this).apply {
            textSize = 20f
            gravity = Gravity.CENTER
            setPadding(0, padding, 0, padding)
        }
        layout.addView(selectedLabel)

        val previous = Button(this).apply {
            text = "Anterior"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(1)) }
        }
        val next = Button(this).apply {
            text = "Siguiente"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(0)) }
        }
        val up = Button(this).apply {
            text = "Arriba"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(4)) }
        }
        val down = Button(this).apply {
            text = "Abajo"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(5)) }
        }
        val left = Button(this).apply {
            text = "Izquierda"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(6)) }
        }
        val right = Button(this).apply {
            text = "Derecha"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(7)) }
        }
        val back = Button(this).apply {
            text = "Volver"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(3)) }
        }
        val confirm = Button(this).apply {
            text = "Confirmar"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(2)) }
        }
        layout.addView(previous)
        layout.addView(next)
        layout.addView(up)
        layout.addView(down)
        layout.addView(left)
        layout.addView(right)
        layout.addView(confirm)
        layout.addView(back)
        commandStatus = TextView(this).apply { setPadding(0, padding, 0, 0) }
        layout.addView(commandStatus)
        setContentView(layout)
        render(RomImportBridge.nativeStartRuntime(filesDir.resolve("ykw2-cache").absolutePath))
        renderBackground()
    }

    override fun onResume() {
        super.onResume()
        frameHandler.post(frameTicker)
    }

    override fun onPause() {
        frameHandler.removeCallbacks(frameTicker)
        super.onPause()
    }

    private fun render(response: String) {
        val values = response.lineSequence()
            .mapNotNull { line ->
                val separator = line.indexOf('=')
                if (separator <= 0) null else line.substring(0, separator) to line.substring(separator + 1)
            }
            .toList()
        val statusKey = values.lastOrNull { it.first == "status" }?.second ?: "runtime.title_menu"
        val screen = values.lastOrNull { it.first == "screen" }?.second ?: "title_menu"
        status.text = statusLabel(statusKey)
        val selected = values.lastOrNull { it.first == "selected" }?.second?.toIntOrNull() ?: 0
        val items = values.filter { it.first == "item" }.map { it.second.split('|') }
        val selectedItem = items.getOrNull(selected)
        selectedLabel.text = selectedItem?.getOrNull(1)?.let(::labelFor) ?: statusLabel(statusKey)
        val command = values.firstOrNull { it.first == "command" }?.second ?: "none"
        val sceneResourceCount = values.count { it.first == "scene_resource" }
        commandStatus.text = when (command) {
            "request_authorized_import" -> "Selecciona una copia autorizada desde el menú de importación."
            "start_new_game" -> "Nueva partida iniciada."
            "open_settings" -> "Las opciones se conectarán en la siguiente fase."
            else -> if (screen == "world") {
                val map = values.lastOrNull { it.first == "world_map" }?.second ?: "mapa pendiente"
                val x = values.lastOrNull { it.first == "player_x" }?.second ?: "0"
                val y = values.lastOrNull { it.first == "player_y" }?.second ?: "0"
                "Mapa: $map\nJugador: ($x, $y)\n$sceneResourceCount recursos de escena cargados."
            } else ""
        }
        if (command == "request_authorized_import") finish()
    }

    private fun renderBackground() {
        val width = RomImportBridge.nativeRuntimeBackgroundWidth()
        val height = RomImportBridge.nativeRuntimeBackgroundHeight()
        val rgba = RomImportBridge.nativeRuntimeBackgroundRgba()
        val expectedBytes = width.toLong() * height.toLong() * 4L
        if (width <= 0 || height <= 0 || expectedBytes > Int.MAX_VALUE ||
            rgba == null || rgba.size.toLong() != expectedBytes) {
            backgroundImage.visibility = View.GONE
            return
        }
        val pixels = IntArray(width * height)
        for (index in pixels.indices) {
            val offset = index * 4
            val red = rgba[offset].toInt() and 0xff
            val green = rgba[offset + 1].toInt() and 0xff
            val blue = rgba[offset + 2].toInt() and 0xff
            val alpha = rgba[offset + 3].toInt() and 0xff
            pixels[index] = Color.argb(alpha, red, green, blue)
        }
        backgroundImage.setImageBitmap(
            Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888),
        )
        backgroundImage.visibility = View.VISIBLE
    }

    private fun labelFor(key: String): String = when (key) {
        "menu.new_game" -> "Nueva partida"
        "menu.continue" -> "Continuar"
        "menu.settings" -> "Opciones"
        else -> key
    }

    private fun statusLabel(key: String): String = when (key) {
        "runtime.title_menu" -> "Contenido inicial validado"
        "runtime.world" -> "Explorando Yo-kai Watch 2"
        "runtime.content_missing" -> "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        "import.required" -> "Selecciona una copia autorizada para importar."
        "import.incomplete" -> "La importación está incompleta; vuelve a seleccionar el dump."
        else -> key
    }
}
