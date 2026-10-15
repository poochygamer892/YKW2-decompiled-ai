package com.ykw2.port

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
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
    private lateinit var dpadControls: LinearLayout
    private lateinit var confirmButton: Button
    private lateinit var battleControls: LinearLayout
    private lateinit var backButton: Button
    private var backgroundBitmap: Bitmap? = null
    private var lastAudioSequence = 0L
    private val toneGenerator = ToneGenerator(AudioManager.STREAM_MUSIC, 80)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val padding = (resources.displayMetrics.density * 16).toInt()
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
            gravity = Gravity.CENTER
        }
        layout.addView(status)
        sceneView = RuntimeSceneView(this)
        layout.addView(sceneView, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            (resources.displayMetrics.density * 360).toInt(),
        ))
        selectedLabel = TextView(this).apply {
            textSize = 20f
            gravity = Gravity.CENTER
            setPadding(0, padding / 2, 0, padding / 2)
        }
        layout.addView(selectedLabel)

        dpadControls = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
        }
        dpadControls.addView(controlButton("▲", 4), LinearLayout.LayoutParams(-2, -2))
        val dpadRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        dpadRow.addView(controlButton("◀", 6), LinearLayout.LayoutParams(0, -2, 1f))
        dpadRow.addView(controlButton("●", 2), LinearLayout.LayoutParams(0, -2, 1f))
        dpadRow.addView(controlButton("▶", 7), LinearLayout.LayoutParams(0, -2, 1f))
        dpadControls.addView(dpadRow, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        dpadControls.addView(controlButton("▼", 5), LinearLayout.LayoutParams(-2, -2))
        layout.addView(dpadControls)

        confirmButton = controlButton("A · Confirmar", 2)
        layout.addView(confirmButton)
        battleControls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        battleControls.addView(controlButton("Atacar", 8), LinearLayout.LayoutParams(0, -2, 1f))
        battleControls.addView(controlButton("Habilidad", 9), LinearLayout.LayoutParams(0, -2, 1f))
        battleControls.addView(controlButton("Huir", 10), LinearLayout.LayoutParams(0, -2, 1f))
        layout.addView(battleControls)
        backButton = controlButton("B · Volver", 3)
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
        toneGenerator.release()
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
        val selectedItem = items.getOrNull(selected)
        val menuItems = items.mapNotNull { it.getOrNull(1)?.let(::labelFor) }
        selectedLabel.text = selectedItem?.getOrNull(1)?.let(::labelFor) ?: when {
            screen == "dialogue" -> "A · Avanzar evento"
            screen == "battle" -> "Comandos reales del catálogo YW2"
            screen == "world" -> "● · Interactuar"
            else -> statusLabel(statusKey, screen)
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
        val speaker = value("speaker")
        val dialogueText = eventDescription(
            eventID = eventID,
            symbol = value("event_symbol"),
            instructions = value("event_instructions", "0").toIntOrNull() ?: 0,
            arguments = value("event_arguments", "0").toIntOrNull() ?: 0,
            decodedInstructions = value("event_decoded_instructions", "0").toIntOrNull() ?: 0,
            lineIndex = value("line_index", "0").toIntOrNull() ?: 0,
            lineCount = value("line_count", "0").toIntOrNull() ?: 0,
            operationToken = value("event_exec_operation"),
            operationStatus = value("event_exec_operation_status", "idle"),
            argumentIndex = value("event_exec_argument_index", "0"),
            argumentCount = value("event_exec_argument_count", "0"),
        )
        val playerX = value("player_x", "0").toFloatOrNull() ?: 0f
        val playerY = value("player_y", "0").toFloatOrNull() ?: 0f
        sceneView.render(
            screen = screen,
            background = backgroundBitmap,
            playerX = playerX,
            playerY = playerY,
            speaker = speaker,
            dialogueText = dialogueText,
            enemyId = value("enemy_id"),
            playerHp = value("player_hp", "0").toIntOrNull() ?: 0,
            playerMaxHp = value("player_max_hp", "1").toIntOrNull() ?: 1,
            enemyHp = value("enemy_hp", "0").toIntOrNull() ?: 0,
            enemyMaxHp = value("enemy_max_hp", "1").toIntOrNull() ?: 1,
            meshVertices = meshVertices,
            meshIndices = meshIndices,
            menuItems = menuItems,
            selectedIndex = selected,
            sceneHeightPercent = sceneHeightPercent,
            dialogueTopPercent = dialogueTopPercent,
        )

        updateControls(screen)

        val command = value("command", "none")
        val sceneResourceCount = values.count { it.first == "scene_resource" }
        val battleDefinition = value("battle_definition_id", "sin definición")
        val battleCommandCount = value("battle_configured_command_count", "0")
        val battleLastCommand = value("battle_last_command_id", "")
        commandStatus.text = when {
            screen == "dialogue" -> dialogueText
            screen == "battle" -> {
                val selectedCommand = if (battleLastCommand.isEmpty()) {
                    "Ningún comando seleccionado"
                } else {
                    "Último comando real: $battleLastCommand"
                }
                "Combate configurado: $battleDefinition\n$battleCommandCount comandos reales detectados.\n$selectedCommand\nFalta interpretar reglas y encuentros para resolver daño, enemigos y victoria."
            }
            command == "request_authorized_import" -> "Selecciona una copia autorizada desde el menú de importación."
            command == "start_new_game" -> "Nueva partida iniciada."
            command == "advance_dialogue" -> "Historia avanzada."
            command == "start_battle" -> "Definición de combate de YW2 cargada."
            command == "battle_won" -> "¡Victoria!"
            command == "battle_escaped" -> "Has escapado del combate."
            command == "battle_defeated" -> "Has sido derrotado."
            command == "open_settings" -> "Las opciones se conectarán en la siguiente fase."
            screen == "world" -> {
                val map = value("world_map", "mapa pendiente")
                "Mapa YW2: $map\nJugador: (${value("player_x")}, ${value("player_y")})\n$sceneResourceCount recursos indexados."
            }
            else -> ""
        }
        playAudioCue(value("audio_sequence", "0").toLongOrNull() ?: 0L, value("audio_cue"))
        if (command == "request_authorized_import") finish()
    }

    private fun updateControls(screen: String) {
        dpadControls.visibility = if (screen == "title_menu" || screen == "world") {
            View.VISIBLE
        } else {
            View.GONE
        }
        confirmButton.visibility = if (screen == "title_menu" || screen == "world" ||
            screen == "dialogue") View.VISIBLE else View.GONE
        battleControls.visibility = if (screen == "battle") View.VISIBLE else View.GONE
        backButton.visibility = if (screen == "world" || screen == "dialogue" ||
            screen == "battle") View.VISIBLE else View.GONE
    }

    private fun playAudioCue(sequence: Long, cue: String) {
        if (sequence <= lastAudioSequence) return
        lastAudioSequence = sequence
        val tone = when {
            cue == "victory" -> ToneGenerator.TONE_PROP_ACK
            cue == "defeat" -> ToneGenerator.TONE_PROP_NACK
            cue == "battle_start" || cue.startsWith("battle_definition:") ->
                ToneGenerator.TONE_PROP_PROMPT
            else -> ToneGenerator.TONE_PROP_BEEP
        }
        toneGenerator.startTone(tone, 120)
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
        symbol: String,
        instructions: Int,
        arguments: Int,
        decodedInstructions: Int,
        lineIndex: Int,
        lineCount: Int,
        operationToken: String,
        operationStatus: String,
        argumentIndex: String,
        argumentCount: String,
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
        return "$eventID\n$step\n$operation"
    }

    private fun labelFor(key: String): String = when (key) {
        "menu.new_game" -> "Nueva partida"
        "menu.continue" -> "Continuar"
        "menu.settings" -> "Opciones"
        else -> key
    }

    private fun statusLabel(key: String, screen: String): String = when {
        screen == "dialogue" -> "Historia"
        screen == "battle" -> "Combate"
        key == "runtime.title_menu" -> "Presentación YW2 · contenido indexado"
        key == "runtime.world" -> "Explorando Yo-kai Watch 2"
        key == "runtime.content_missing" -> "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        key == "import.required" -> "Selecciona una copia autorizada para importar."
        key == "import.incomplete" -> "La importación está incompleta; vuelve a seleccionar el dump."
        else -> key
    }
}
