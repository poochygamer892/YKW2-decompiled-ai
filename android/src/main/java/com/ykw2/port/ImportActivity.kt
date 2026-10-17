package com.ykw2.port

import android.app.Activity
import android.app.AlertDialog
import android.graphics.Bitmap
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.TextView
/** First-launch menu for importing a user-selected, authorized 3DS image. */
class ImportActivity : Activity() {
    private lateinit var status: TextView
    private lateinit var selectButton: Button
    private lateinit var runtimeButton: Button
    private lateinit var rightsCheck: CheckBox

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(createView())
        refreshImportState()
    }

    private fun createView(): View {
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
        layout.addView(TextView(this).apply {
            text = "Importar una copia autorizada"
            textSize = 18f
            setPadding(0, padding / 2, 0, padding / 2)
        })

        rightsCheck = CheckBox(this).apply {
            text = "Confirmo que tengo derecho a usar esta copia en este dispositivo"
            setOnCheckedChangeListener { _, checked -> selectButton.isEnabled = checked }
        }
        layout.addView(rightsCheck)

        selectButton = Button(this).apply {
            text = "Seleccionar ROM"
            isEnabled = false
            setOnClickListener { openDocumentPicker() }
        }
        layout.addView(selectButton)

        runtimeButton = Button(this).apply {
            text = "Iniciar Yo-kai Watch 2"
            visibility = View.GONE
            setOnClickListener {
                val romUri = getPreferences(MODE_PRIVATE).getString(KEY_ROM_URI, null)
                if (romUri == null) {
                    status.text = "Vuelve a seleccionar la ROM para abrirla en Zakuro."
                } else {
                    startActivity(Intent(this@ImportActivity, ZakuroRuntimeActivity::class.java)
                        .putExtra(ZakuroRuntimeActivity.EXTRA_ROM_URI, romUri))
                }
            }
        }
        layout.addView(runtimeButton)

        layout.addView(Button(this).apply {
            text = MobileCheats.buttonTitle(this@ImportActivity)
            setOnClickListener {
                val choices = MobileCheats.fpsModes +
                    "Velocidad normal: ${if (MobileCheats.normalSpeed(this@ImportActivity)) "activada" else "desactivada"} (alternar)"
                AlertDialog.Builder(this@ImportActivity)
                    .setTitle("Cheats de rendimiento")
                    .setItems(choices) { _, which ->
                        if (which < MobileCheats.fpsModes.size) {
                            MobileCheats.setFpsMode(this@ImportActivity, which)
                        } else {
                            MobileCheats.toggleNormalSpeed(this@ImportActivity)
                        }
                        text = MobileCheats.buttonTitle(this@ImportActivity)
                    }
                    .setNegativeButton("Cancelar", null)
                    .show()
            }
        })

        layout.addView(Button(this).apply {
            text = "Configurar controles táctiles"
            setOnClickListener {
                startActivity(Intent(this@ImportActivity, TouchControlsActivity::class.java))
            }
        })

        status = TextView(this).apply {
            text = "Selecciona una copia local autorizada para comenzar."
            setPadding(0, padding, 0, 0)
        }
        layout.addView(status)
        return layout
    }

    private fun openDocumentPicker() {
        startActivityForResult(
            Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            },
            REQUEST_ROM,
        )
    }

    @Deprecated("Use Activity Result APIs when the Android app module is initialized")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_ROM || resultCode != RESULT_OK) return
        val uri = data?.data ?: run {
            status.text = "No se seleccionó ningún archivo."
            return
        }
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                (data?.flags ?: 0) and Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        } catch (_: SecurityException) {
            // Some document providers do not offer persistable permissions.
        }
        getPreferences(MODE_PRIVATE).edit()
            .putString(KEY_ROM_URI, uri.toString())
            .apply()
        refreshImportState()
    }

    private fun refreshImportState() {
        val hasRom = getPreferences(MODE_PRIVATE).contains(KEY_ROM_URI)
        runtimeButton.visibility = if (hasRom && RomImportBridge.available) View.VISIBLE else View.GONE
        status.text = when {
            !RomImportBridge.available -> "El host nativo Zakuro no está disponible en esta instalación."
            hasRom -> "ROM seleccionada. Zakuro la abrirá directamente al iniciar; no hace falta extraer la RomFS."
            else -> "Selecciona tu copia local autorizada para abrirla en Zakuro."
        }
    }

    companion object {
        private const val REQUEST_ROM = 1001
        private const val KEY_ROM_URI = "authorized_rom_uri"
    }
}

object RomImportBridge {
    var available: Boolean = false
        private set

    init {
        try {
            System.loadLibrary("ykw2_import")
            available = true
        } catch (_: UnsatisfiedLinkError) {
            // The app can still render the menu while the native module is built.
        }
    }

    external fun nativeValidateRom(fileDescriptor: Int, imageSize: Long): Int
    external fun nativeImportRom(fileDescriptor: Int, imageSize: Long, cacheDirectory: String): Int
    external fun nativeCheckImport(cacheDirectory: String): Int
    external fun nativeStartRuntime(cacheDirectory: String): String
    external fun nativeRuntimeInput(input: Int): String
    external fun nativeRuntimeAdvance(deltaSeconds: Double): String
    external fun nativeRuntimeBackgroundWidth(): Int
    external fun nativeRuntimeBackgroundHeight(): Int
    external fun nativeRuntimeBackgroundRgba(): ByteArray?

    external fun nativeEmulatorOpen(romPath: String, recompiledLibrary: String?, dataDirectory: String): String
    external fun nativeEmulatorRunFrames(frames: Int): Int
    external fun nativeEmulatorSetInput(
        buttons: Int,
        circleX: Float,
        circleY: Float,
        touchX: Int,
        touchY: Int,
    ): Int
    external fun nativeEmulatorRenderScreens(top: Bitmap, bottom: Bitmap): Int
    external fun nativeEmulatorTakeAudio(samples: ShortArray): Int
    external fun nativeEmulatorClose()
}
