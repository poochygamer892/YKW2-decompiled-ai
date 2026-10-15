package com.ykw2.port

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/** Native Android host for the portable RuntimeScene snapshot. */
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
    private lateinit var sceneView: RuntimeSceneView
    private lateinit var menuPanel: LinearLayout
    private val menuButtons = mutableListOf<Button>()
    private lateinit var confirmButton: Button
    private lateinit var battleControls: LinearLayout
    private lateinit var backButton: Button
    private var backgroundBitmap: Bitmap? = null
    private var lastMenuState = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val padding = (resources.displayMetrics.density * 16).toInt()
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(padding, padding, padding, padding)
        }
        status = TextView(this).apply {
            setPadding(0, padding / 2, 0, padding / 2)
            gravity = Gravity.CENTER
        }
        layout.addView(status)
        sceneView = RuntimeSceneView(this)
        val sceneFrame = FrameLayout(this)
        sceneFrame.addView(sceneView, FrameLayout.LayoutParams(-1, -1))
        menuPanel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding / 2, padding / 2, padding / 2, padding / 2)
            background = GradientDrawable().apply {
                setColor(Color.argb(228, 9, 20, 31))
                cornerRadius = padding.toFloat()
            }
        }
        for (index in 0 until 4) {
            val button = Button(this).apply {
                isAllCaps = false
                textSize = 15f
                minHeight = (resources.displayMetrics.density * 48).toInt()
                setOnClickListener {
                    render(RomImportBridge.nativeRuntimeMenuSelect(index))
                }
            }
            menuPanel.addView(button, LinearLayout.LayoutParams(-1, -2))
            menuButtons.add(button)
        }
        sceneFrame.addView(menuPanel, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM,
        ))
        layout.addView(sceneFrame, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            (resources.displayMetrics.density * 460).toInt(),
        ))
        selectedLabel = TextView(this).apply {
            textSize = 20f
            gravity = Gravity.CENTER
            setPadding(0, padding / 2, 0, padding / 2)
        }
        layout.addView(selectedLabel)

        confirmButton = controlButton("Siguiente paso", 2)
        layout.addView(confirmButton)
        battleControls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        battleControls.addView(controlButton("Comando 1", 8), LinearLayout.LayoutParams(0, -2, 1f))
        battleControls.addView(controlButton("Comando 2", 9), LinearLayout.LayoutParams(0, -2, 1f))
        layout.addView(battleControls)
        backButton = controlButton("Volver al menú", 3)
        layout.addView(backButton)
        commandStatus = TextView(this).apply {
            setPadding(0, padding, 0, padding)
            gravity = Gravity.CENTER
        }
        layout.addView(commandStatus)

        val scroll = ScrollView(this)
        scroll.addView(layout)
        setContentView(scroll)
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

    override fun onDestroy() {
        frameHandler.removeCallbacks(frameTicker)
        super.onDestroy()
    }

    private fun controlButton(label: String, input: Int): Button = Button(this).apply {
        text = label
        setOnClickListener { render(RomImportBridge.nativeRuntimeInput(input)) }
    }

    private fun render(response: String) {
        val values = response.lineSequence()
            .mapNotNull { line ->
                val separator = line.indexOf('=')
                if (separator <= 0) null else line.substring(0, separator) to line.substring(separator + 1)
            }
            .toList()
        fun value(key: String, fallback: String = ""): String =
            values.lastOrNull { it.first == key }?.second ?: fallback

        val statusKey = value("status", "runtime.title_menu")
        val screen = value("screen", "title_menu")
        val sceneHeightPercent = value("presentation_scene_height_percent", "58").toFloatOrNull() ?: 58f
        val dialogueTopPercent = value("presentation_dialogue_top_percent", "55").toFloatOrNull() ?: 55f
        status.text = statusLabel(statusKey, screen)
        val selected = value("selected", "0").toIntOrNull() ?: 0
        val items = values.filter { it.first == "item" }.map { it.second.split('|') }
        updateMenu(items, selected)
        selectedLabel.text = when (screen) {
            "dialogue" -> "Avance estructural del evento; todavía no reproduce la escena"
            "battle" -> "Selecciona un ID del catálogo; no se ejecuta un combate"
            else -> ""
        }

        val meshVertices = values.filter { it.first == "scene_mesh_vertex" }.mapNotNull { entry ->
            val parts = entry.second.split('|')
            if (parts.size != 4) return@mapNotNull null
            val x = parts[1].toFloatOrNull() ?: return@mapNotNull null
            val y = parts[2].toFloatOrNull() ?: return@mapNotNull null
            val z = parts[3].toFloatOrNull() ?: return@mapNotNull null
            floatArrayOf(x, y, z)
        }
        val meshIndices = values.filter { it.first == "scene_mesh_index" }.mapNotNull { entry ->
            entry.second.substringAfterLast('|').toIntOrNull()
        }

        val eventID = value("event_id")
        val dialogueText = eventDescription(
            eventID = eventID,
            instructions = value("event_instructions", "0").toIntOrNull() ?: 0,
            lineIndex = value("line_index", "0").toIntOrNull() ?: 0,
            lineCount = value("line_count", "0").toIntOrNull() ?: 0,
            operationStatus = value("event_exec_operation_status", "idle"),
        )
        val playerX = value("player_x", "0").toFloatOrNull() ?: 0f
        val playerY = value("player_y", "0").toFloatOrNull() ?: 0f
        sceneView.render(
            screen = screen,
            background = backgroundBitmap,
            playerX = playerX,
            playerY = playerY,
            speaker = "",
            dialogueText = dialogueText,
            enemyId = value("enemy_id"),
            playerHp = value("player_hp", "0").toIntOrNull() ?: 0,
            playerMaxHp = value("player_max_hp", "1").toIntOrNull() ?: 1,
            enemyHp = value("enemy_hp", "0").toIntOrNull() ?: 0,
            enemyMaxHp = value("enemy_max_hp", "1").toIntOrNull() ?: 1,
            meshVertices = meshVertices,
            meshIndices = meshIndices,
            sceneHeightPercent = sceneHeightPercent,
            dialogueTopPercent = dialogueTopPercent,
        )

        updateControls(screen)

        val command = value("command", "none")
        val battleDefinition = value("battle_definition_id", "sin definición")
        val battleCommandCount = value("battle_configured_command_count", "0")
        val battleLastCommand = value("battle_last_command_id", "")
        commandStatus.text = when {
            screen == "dialogue" -> "Programa: $eventID\nLa semántica de esta escena sigue pendiente."
            screen == "battle" -> {
                val selectedCommand = if (battleLastCommand.isEmpty()) {
                    "Ningún ID seleccionado"
                } else {
                    "ID seleccionado: $battleLastCommand"
                }
                "$battleDefinition · $battleCommandCount IDs indexados\n$selectedCommand"
            }
            command == "request_authorized_import" -> "Selecciona una copia autorizada desde el menú de importación."
            screen == "title_menu" -> "Nueva partida y Continuar se activarán cuando historia, mapa y guardado funcionen con datos YW2."
            else -> ""
        }
        if (command == "request_authorized_import") finish()
    }

    private fun updateMenu(items: List<List<String>>, selected: Int) {
        val state = items.joinToString(";") { it.joinToString("|") } + ":$selected"
        if (state == lastMenuState) return
        lastMenuState = state
        menuButtons.forEachIndexed { index, button ->
            val item = items.getOrNull(index)
            val enabled = item?.getOrNull(2) == "1"
            button.text = item?.getOrNull(1)?.let(::labelFor) ?: ""
            button.isEnabled = enabled
            button.visibility = if (item == null) View.GONE else View.VISIBLE
            button.setTextColor(if (enabled) Color.WHITE else Color.rgb(158, 174, 184))
            button.background = GradientDrawable().apply {
                setColor(if (index == selected && enabled) Color.rgb(24, 113, 142)
                    else Color.rgb(22, 40, 53))
                cornerRadius = resources.displayMetrics.density * 12
                setStroke((resources.displayMetrics.density * 1).toInt(),
                    if (enabled) Color.rgb(67, 172, 198) else Color.rgb(58, 75, 85))
            }
        }
    }

    private fun updateControls(screen: String) {
        menuPanel.visibility = if (screen == "title_menu") View.VISIBLE else View.GONE
        confirmButton.visibility = if (screen == "dialogue") View.VISIBLE else View.GONE
        battleControls.visibility = if (screen == "battle") View.VISIBLE else View.GONE
        backButton.visibility = if (screen == "dialogue" ||
            screen == "battle") View.VISIBLE else View.GONE
    }

    private fun renderBackground() {
        val width = RomImportBridge.nativeRuntimeBackgroundWidth()
        val height = RomImportBridge.nativeRuntimeBackgroundHeight()
        val rgba = RomImportBridge.nativeRuntimeBackgroundRgba()
        val expectedBytes = width.toLong() * height.toLong() * 4L
        if (width <= 0 || height <= 0 || expectedBytes > Int.MAX_VALUE ||
            rgba == null || rgba.size.toLong() != expectedBytes) return
        val pixels = IntArray(width * height)
        for (index in pixels.indices) {
            val offset = index * 4
            pixels[index] = Color.argb(
                rgba[offset + 3].toInt() and 0xff,
                rgba[offset].toInt() and 0xff,
                rgba[offset + 1].toInt() and 0xff,
                rgba[offset + 2].toInt() and 0xff,
            )
        }
        backgroundBitmap = Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888)
    }

    private fun eventDescription(
        eventID: String,
        instructions: Int,
        lineIndex: Int,
        lineCount: Int,
        operationStatus: String,
    ): String {
        if (eventID.isEmpty()) return "No se encontró un evento de apertura compatible en el dump."
        val total = if (lineCount > 0) lineCount else instructions
        val step = if (total > 0) "Paso ${lineIndex + 1}/$total" else "Programa sin pasos"
        val operation = when (operationStatus) {
            "unknown_operation" -> "Paso estructural; operación aún no traducida"
            "invalid_arguments" -> "Paso detenido: argumentos no validados"
            "completed" -> "Evento completado"
            else -> "Paso de evento"
        }
        return "$step\n$operation"
    }

    private fun labelFor(key: String): String = when (key) {
        "menu.new_game" -> "Nueva partida"
        "menu.continue" -> "Continuar"
        "menu.inspect_opening" -> "Inspeccionar apertura YW2"
        "menu.battle_catalog" -> "Consultar catálogo de combate"
        else -> key
    }

    private fun statusLabel(key: String, screen: String): String = when {
        screen == "dialogue" -> "Evento de apertura · inspección"
        screen == "battle" -> "Datos de combate · catálogo"
        key == "runtime.title_menu" -> "Yo-kai Watch 2 · prototipo móvil"
        key == "runtime.content_missing" -> "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        key == "import.required" -> "Selecciona una copia autorizada para importar."
        key == "import.incomplete" -> "La importación está incompleta; vuelve a seleccionar el dump."
        else -> key
    }
}
