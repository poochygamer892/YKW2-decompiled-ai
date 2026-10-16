package com.ykw2.port

import android.content.Context

/** Per-orientation, normalized touchscreen layout shared by gameplay and its editor. */
internal data class TouchControlLayout(
    val profile: String,
    var scale: Float = 1f,
    var leftX: Float = 0.15f,
    var dpadY: Float = 0.65f,
    var stickX: Float = 0.15f,
    var stickY: Float = 0.84f,
    var faceX: Float = 0.85f,
    var faceY: Float = 0.77f,
    var utilityX: Float = 0.84f,
    var utilityY: Float = 0.58f,
    var leftShoulderX: Float = 0.09f,
    var leftShoulderY: Float = 0.11f,
    var rightShoulderX: Float = 0.91f,
    var rightShoulderY: Float = 0.11f,
    var sizes: MutableMap<String, Float> = defaultSizes(),
) {
    fun save(context: Context) {
        val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
        val prefix = "${profile}_"
        preferences.edit()
            .putFloat(prefix + "scale", scale)
            .putFloat(prefix + "leftX", leftX)
            .putFloat(prefix + "dpadY", dpadY)
            .putFloat(prefix + "stickX", stickX)
            .putFloat(prefix + "stickY", stickY)
            .putFloat(prefix + "faceX", faceX)
            .putFloat(prefix + "faceY", faceY)
            .putFloat(prefix + "utilityX", utilityX)
            .putFloat(prefix + "utilityY", utilityY)
            .putFloat(prefix + "leftShoulderX", leftShoulderX)
            .putFloat(prefix + "leftShoulderY", leftShoulderY)
            .putFloat(prefix + "rightShoulderX", rightShoulderX)
            .putFloat(prefix + "rightShoulderY", rightShoulderY)
            .also { editor -> sizes.forEach { (key, value) -> editor.putFloat(prefix + "size_" + key, value) } }
            .apply()
    }

    fun size(control: String): Float = sizes[control] ?: 1f

    fun setSize(control: String, value: Float) {
        sizes[control] = value.coerceIn(MIN_SIZE, MAX_SIZE)
    }

    companion object {
        private const val PREFERENCES = "touch_controls"

        fun defaults(width: Int, height: Int): TouchControlLayout {
            val profile = if (height >= width) "portrait" else "landscape"
            return TouchControlLayout(profile).apply {
                if (profile == "landscape") {
                    dpadY = 0.32f
                    stickY = 0.75f
                    faceY = 0.72f
                }
            }
        }

        fun load(context: Context, width: Int, height: Int): TouchControlLayout {
            val defaults = defaults(width, height)
            val profile = defaults.profile
            val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            val prefix = "${profile}_"
            val loadedSizes = defaults.sizes.toMutableMap()
            loadedSizes.keys.forEach { key ->
                loadedSizes[key] = preferences.getFloat(prefix + "size_" + key, 1f).coerceIn(MIN_SIZE, MAX_SIZE)
            }
            return defaults.copy(
                scale = preferences.getFloat(prefix + "scale", defaults.scale).coerceIn(MIN_SIZE, MAX_SIZE),
                leftX = preferences.getFloat(prefix + "leftX", defaults.leftX).coerceIn(0.04f, 0.96f),
                dpadY = preferences.getFloat(prefix + "dpadY", defaults.dpadY).coerceIn(0.05f, 0.95f),
                stickX = preferences.getFloat(prefix + "stickX", defaults.stickX).coerceIn(0.04f, 0.96f),
                stickY = preferences.getFloat(prefix + "stickY", defaults.stickY).coerceIn(0.05f, 0.95f),
                faceX = preferences.getFloat(prefix + "faceX", defaults.faceX).coerceIn(0.04f, 0.96f),
                faceY = preferences.getFloat(prefix + "faceY", defaults.faceY).coerceIn(0.05f, 0.95f),
                utilityX = preferences.getFloat(prefix + "utilityX", defaults.utilityX).coerceIn(0.04f, 0.96f),
                utilityY = preferences.getFloat(prefix + "utilityY", defaults.utilityY).coerceIn(0.05f, 0.95f),
                leftShoulderX = preferences.getFloat(prefix + "leftShoulderX", defaults.leftShoulderX).coerceIn(0.04f, 0.96f),
                leftShoulderY = preferences.getFloat(prefix + "leftShoulderY", defaults.leftShoulderY).coerceIn(0.05f, 0.95f),
                rightShoulderX = preferences.getFloat(prefix + "rightShoulderX", defaults.rightShoulderX).coerceIn(0.04f, 0.96f),
                rightShoulderY = preferences.getFloat(prefix + "rightShoulderY", defaults.rightShoulderY).coerceIn(0.05f, 0.95f),
                sizes = loadedSizes,
            )
        }

        const val ALL = "ALL"
        const val MIN_SIZE = 0.5f
        const val MAX_SIZE = 2.0f
        private fun defaultSizes() = mutableMapOf(
            "UP" to 1f, "DOWN" to 1f, "LEFT" to 1f, "RIGHT" to 1f,
            "STICK" to 1f, "A" to 1f, "B" to 1f, "X" to 1f, "Y" to 1f,
            "SELECT" to 1f, "START" to 1f, "L" to 1f, "R" to 1f,
        )
    }
}
