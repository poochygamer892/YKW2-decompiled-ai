package com.ykw2.port

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/** Minimal native host for the portable RuntimeScene snapshot. */
class RuntimeMenuActivity : Activity() {
    private lateinit var selectedLabel: TextView
    private lateinit var status: TextView
    private lateinit var commandStatus: TextView

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
        val confirm = Button(this).apply {
            text = "Confirmar"
            setOnClickListener { render(RomImportBridge.nativeRuntimeInput(2)) }
        }
        layout.addView(previous)
        layout.addView(next)
        layout.addView(confirm)
        commandStatus = TextView(this).apply { setPadding(0, padding, 0, 0) }
        layout.addView(commandStatus)
        setContentView(layout)
        render(RomImportBridge.nativeStartRuntime(filesDir.resolve("ykw2-cache").absolutePath))
    }

    private fun render(response: String) {
        val values = response.lineSequence()
            .mapNotNull { line ->
                val separator = line.indexOf('=')
                if (separator <= 0) null else line.substring(0, separator) to line.substring(separator + 1)
            }
            .toList()
        val statusKey = values.lastOrNull { it.first == "status" }?.second ?: "runtime.title_menu"
        status.text = statusLabel(statusKey)
        val selected = values.lastOrNull { it.first == "selected" }?.second?.toIntOrNull() ?: 0
        val items = values.filter { it.first == "item" }.map { it.second.split('|') }
        val selectedItem = items.getOrNull(selected)
        selectedLabel.text = selectedItem?.getOrNull(1)?.let(::labelFor) ?: statusLabel(statusKey)
        val command = values.firstOrNull { it.first == "command" }?.second ?: "none"
        val sceneResourceCount = values.count { it.first == "scene_resource" }
        commandStatus.text = when (command) {
            "request_authorized_import" -> "Selecciona una copia autorizada desde el menú de importación."
            "start_new_game" -> "Nueva partida: $sceneResourceCount recursos base listos para el renderer."
            "open_settings" -> "Las opciones se conectarán en la siguiente fase."
            else -> ""
        }
        if (command == "request_authorized_import") finish()
    }

    private fun labelFor(key: String): String = when (key) {
        "menu.new_game" -> "Nueva partida"
        "menu.continue" -> "Continuar"
        "menu.settings" -> "Opciones"
        else -> key
    }

    private fun statusLabel(key: String): String = when (key) {
        "runtime.title_menu" -> "Contenido inicial validado"
        "runtime.content_missing" -> "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        "import.required" -> "Selecciona una copia autorizada para importar."
        "import.incomplete" -> "La importación está incompleta; vuelve a seleccionar el dump."
        else -> key
    }
}
