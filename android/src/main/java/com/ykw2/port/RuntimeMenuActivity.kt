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
            (resources.displayMetrics.density * 280).toInt(),
        ))
        selectedLabel = TextView(this).apply {
            textSize = 20f
            gravity = Gravity.CENTER
            setPadding(0, padding / 2, 0, padding / 2)
        }
        layout.addView(selectedLabel)

        layout.addView(controlButton("Anterior", 1))
        layout.addView(controlButton("Siguiente", 0))
        layout.addView(controlButton("Arriba", 4))
        layout.addView(controlButton("Abajo", 5))
        layout.addView(controlButton("Izquierda", 6))
        layout.addView(controlButton("Derecha", 7))
        layout.addView(controlButton("Confirmar / Interactuar", 2))
        val battleControls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        battleControls.addView(controlButton("Atacar", 8), LinearLayout.LayoutParams(0, -2, 1f))
        battleControls.addView(controlButton("Habilidad", 9), LinearLayout.LayoutParams(0, -2, 1f))
        battleControls.addView(controlButton("Huir", 10), LinearLayout.LayoutParams(0, -2, 1f))
        layout.addView(battleControls)
        layout.addView(controlButton("Volver", 3))
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
        status.text = statusLabel(statusKey, screen)
        val selected = value("selected", "0").toIntOrNull() ?: 0
        val items = values.filter { it.first == "item" }.map { it.second.split('|') }
        val selectedItem = items.getOrNull(selected)
        selectedLabel.text = selectedItem?.getOrNull(1)?.let(::labelFor)
            ?: if (screen == "dialogue") "Pulsa Confirmar para continuar"
            else if (screen == "battle") "Elige una acción"
            else statusLabel(statusKey, screen)

        val eventID = value("event_id")
        val speaker = value("speaker")
        val dialogueText = eventDescription(
            eventID = eventID,
            symbol = value("event_symbol"),
            instructions = value("event_instructions", "0").toIntOrNull() ?: 0,
            arguments = value("event_arguments", "0").toIntOrNull() ?: 0,
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
        )

        val command = value("command", "none")
        val sceneResourceCount = values.count { it.first == "scene_resource" }
        commandStatus.text = when {
            screen == "dialogue" -> dialogueText
            screen == "battle" -> "El intérprete de reglas y encuentros de YW2 aún no está disponible."
            command == "request_authorized_import" -> "Selecciona una copia autorizada desde el menú de importación."
            command == "start_new_game" -> "Nueva partida iniciada."
            command == "advance_dialogue" -> "Historia avanzada."
            command == "start_battle" -> "El motor de combate de YW2 todavía no está disponible."
            command == "battle_won" -> "¡Victoria!"
            command == "battle_escaped" -> "Has escapado del combate."
            command == "battle_defeated" -> "Has sido derrotado."
            command == "open_settings" -> "Las opciones se conectarán en la siguiente fase."
            screen == "world" -> {
                val map = value("world_map", "mapa pendiente")
                "Mapa: $map\nJugador: (${value("player_x")}, ${value("player_y")})\n$sceneResourceCount recursos cargados. Pulsa Confirmar para combatir."
            }
            else -> ""
        }
        playAudioCue(value("audio_sequence", "0").toLongOrNull() ?: 0L, value("audio_cue"))
        if (command == "request_authorized_import") finish()
    }

    private fun playAudioCue(sequence: Long, cue: String) {
        if (sequence <= lastAudioSequence) return
        lastAudioSequence = sequence
        val tone = when (cue) {
            "victory" -> ToneGenerator.TONE_PROP_ACK
            "defeat" -> ToneGenerator.TONE_PROP_NACK
            "battle_start" -> ToneGenerator.TONE_PROP_PROMPT
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

    private fun eventDescription(eventID: String, symbol: String, instructions: Int, arguments: Int): String {
        if (eventID.isEmpty()) return "No se encontró un evento de apertura compatible en el dump."
        val entry = if (symbol.isEmpty()) "sin símbolo recuperado" else symbol
        return "Programa YW2 cargado (sin ejecutar)\n$eventID\nEntrada: $entry · $instructions instrucciones · $arguments argumentos\nFalta el intérprete XQ32 para reproducir el evento real."
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
        key == "runtime.title_menu" -> "Contenido inicial validado"
        key == "runtime.world" -> "Explorando Yo-kai Watch 2"
        key == "runtime.content_missing" -> "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        key == "import.required" -> "Selecciona una copia autorizada para importar."
        key == "import.incomplete" -> "La importación está incompleta; vuelve a seleccionar el dump."
        else -> key
    }
}
