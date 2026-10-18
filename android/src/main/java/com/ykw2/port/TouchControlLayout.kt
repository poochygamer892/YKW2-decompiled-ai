package com.ykw2.port

import android.content.Context

/** Per-orientation, normalized touchscreen layout shared by gameplay and its editor. */
internal data class TouchControlLayout(
    val profile: String,
    var scale: Float = 1f,
    var leftX: Float = 0.15f,
    var dpadY: Float = 0.38f,
    var stickX: Float = 0.15f,
    var stickY: Float = 0.47f,
    var faceX: Float = 0.85f,
    var faceY: Float = 0.43f,
    var utilityX: Float = 0.84f,
    var utilityY: Float = 0.38f,
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
            .putInt(prefix + "layoutVersion", LAYOUT_VERSION)
            .also { editor -> sizes.forEach { (key, value) -> editor.putFloat(prefix + "size_" + key, value) } }
            .apply()
    }

    fun duplicate(): TouchControlLayout = copy(sizes = sizes.toMutableMap())

    fun size(control: String): Float = sizes[control] ?: 1f

    fun setSize(control: String, value: Float) {
        sizes[control] = value.coerceIn(MIN_SIZE, MAX_SIZE)
    }

    companion object {
        private const val PREFERENCES = "touch_controls"
        private const val LAYOUT_VERSION = 2

        fun defaults(width: Int, height: Int): TouchControlLayout {
            val profile = if (height >= width) "portrait" else "landscape"
            return TouchControlLayout(profile).apply {
                if (profile == "landscape") {
                    dpadY = 0.32f
                    stickY = 0.75f
                    faceY = 0.72f
                    utilityY = 0.58f
                }
            }
        }

        fun load(context: Context, width: Int, height: Int): TouchControlLayout {
            val defaults = defaults(width, height)
            val profile = defaults.profile
            val preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            val prefix = "${profile}_"
            val hasCurrentLayout = preferences.getInt(prefix + "layoutVersion", 0) >= LAYOUT_VERSION
            val loadedSizes = defaults.sizes.toMutableMap()
            loadedSizes.keys.forEach { key ->
                loadedSizes[key] = preferences.getFloat(prefix + "size_" + key, 1f).coerceIn(MIN_SIZE, MAX_SIZE)
            }
            return defaults.copy(
                scale = preferences.getFloat(prefix + "scale", defaults.scale).coerceIn(MIN_SIZE, MAX_SIZE),
                leftX = if (hasCurrentLayout) preferences.getFloat(prefix + "leftX", defaults.leftX).coerceIn(0.04f, 0.96f) else defaults.leftX,
                dpadY = if (hasCurrentLayout) preferences.getFloat(prefix + "dpadY", defaults.dpadY).coerceIn(0.05f, 0.95f) else defaults.dpadY,
                stickX = if (hasCurrentLayout) preferences.getFloat(prefix + "stickX", defaults.stickX).coerceIn(0.04f, 0.96f) else defaults.stickX,
                stickY = if (hasCurrentLayout) preferences.getFloat(prefix + "stickY", defaults.stickY).coerceIn(0.05f, 0.95f) else defaults.stickY,
                faceX = if (hasCurrentLayout) preferences.getFloat(prefix + "faceX", defaults.faceX).coerceIn(0.04f, 0.96f) else defaults.faceX,
                faceY = if (hasCurrentLayout) preferences.getFloat(prefix + "faceY", defaults.faceY).coerceIn(0.05f, 0.95f) else defaults.faceY,
                utilityX = if (hasCurrentLayout) preferences.getFloat(prefix + "utilityX", defaults.utilityX).coerceIn(0.04f, 0.96f) else defaults.utilityX,
                utilityY = if (hasCurrentLayout) preferences.getFloat(prefix + "utilityY", defaults.utilityY).coerceIn(0.05f, 0.95f) else defaults.utilityY,
                leftShoulderX = if (hasCurrentLayout) preferences.getFloat(prefix + "leftShoulderX", defaults.leftShoulderX).coerceIn(0.04f, 0.96f) else defaults.leftShoulderX,
                leftShoulderY = if (hasCurrentLayout) preferences.getFloat(prefix + "leftShoulderY", defaults.leftShoulderY).coerceIn(0.05f, 0.95f) else defaults.leftShoulderY,
                rightShoulderX = if (hasCurrentLayout) preferences.getFloat(prefix + "rightShoulderX", defaults.rightShoulderX).coerceIn(0.04f, 0.96f) else defaults.rightShoulderX,
                rightShoulderY = if (hasCurrentLayout) preferences.getFloat(prefix + "rightShoulderY", defaults.rightShoulderY).coerceIn(0.05f, 0.95f) else defaults.rightShoulderY,
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
