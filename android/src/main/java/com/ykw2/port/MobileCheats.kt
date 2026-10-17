package com.ykw2.port

import android.content.Context
import java.io.File

/** Installs the bundled Zakuro cheat list and applies the user's saved choices. */
internal object MobileCheats {
    private const val PREFERENCES = "ykw2.game.options"
    private const val FPS_MODE = "fps_mode"
    private const val NORMAL_SPEED = "normal_speed"
    private const val CHEAT_FILE = "00040000001B2900.txt"

    val fpsModes = arrayOf(
        "Desactivado",
        "60 FPS (mantener R para 30 FPS)",
        "60 FPS (FPS++)",
    )

    fun fpsMode(context: Context): Int =
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .getInt(FPS_MODE, 0).coerceIn(fpsModes.indices)

    fun setFpsMode(context: Context, mode: Int) {
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .edit().putInt(FPS_MODE, mode.coerceIn(fpsModes.indices)).apply()
    }

    fun normalSpeed(context: Context): Boolean =
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .getBoolean(NORMAL_SPEED, false)

    fun toggleNormalSpeed(context: Context): Boolean {
        val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
        val enabled = !preferences.getBoolean(NORMAL_SPEED, false)
        preferences.edit().putBoolean(NORMAL_SPEED, enabled).apply()
        return enabled
    }

    fun buttonTitle(context: Context): String {
        val fps = when (fpsMode(context)) {
            1 -> "60 FPS · R=30"
            2 -> "60 FPS++"
            else -> "FPS desactivados"
        }
        val speed = if (normalSpeed(context)) " · velocidad normal" else ""
        return "Cheats de rendimiento · $fps$speed"
    }

    fun install(context: Context, dataDirectory: File) {
        val managedNames = setOf(
            "60FPS (Hold R button for 30FPS)",
            "60FPS (FPS++)",
            "Game Speed Normal",
        )
        val bundled = context.assets.open(CHEAT_FILE).bufferedReader(Charsets.UTF_8).use { it.readText() }
        val file = File(File(dataDirectory, "cheats"), CHEAT_FILE)
        file.parentFile?.mkdirs()
        val existing = if (file.isFile) file.readText(Charsets.UTF_8) else ""

        // Keep any user-added cheat sections, replacing only the three bundled entries.
        val retained = mutableListOf<String>()
        var skipManagedSection = false
        existing.lineSequence().forEach { line ->
            val trimmed = line.trim()
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                skipManagedSection = trimmed.substring(1, trimmed.length - 1) in managedNames
            }
            if (!skipManagedSection) retained += line
        }

        val enabledNames = buildSet {
            when (fpsMode(context)) {
                1 -> add("60FPS (Hold R button for 30FPS)")
                2 -> add("60FPS (FPS++)")
            }
            if (normalSpeed(context)) add("Game Speed Normal")
        }
        val configured = mutableListOf<String>()
        var currentSection = ""
        bundled.lineSequence().forEach { line ->
            configured += line
            val trimmed = line.trim()
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                currentSection = trimmed.substring(1, trimmed.length - 1)
                if (currentSection in enabledNames) configured += "*citra_enabled"
            }
        }

        val preservedText = retained.joinToString("\n").trimEnd()
        val bundledText = configured.joinToString("\n").trimEnd()
        file.writeText(listOf(preservedText, bundledText).filter { it.isNotEmpty() }.joinToString("\n\n") + "\n", Charsets.UTF_8)
    }
}
