package com.ykw2.port

import android.app.Activity
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.TextView
import kotlin.concurrent.thread

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
            text = "Abrir menú inicial"
            visibility = View.GONE
            setOnClickListener {
                startActivity(Intent(this@ImportActivity, RuntimeMenuActivity::class.java))
            }
        }
        layout.addView(runtimeButton)

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
        importSelectedRom(uri)
    }

    private fun importSelectedRom(uri: Uri) {
        val size = querySize(uri)
        if (size <= 0L) {
            status.text = "No se pudo leer el tamaño de la imagen."
            return
        }
        if (!RomImportBridge.available) {
            status.text = "El módulo nativo de importación todavía no está instalado."
            return
        }
        selectButton.isEnabled = false
        status.text = "Validando y extrayendo recursos; no cierres la aplicación..."
        thread(name = "ykw2-rom-import") {
            val validationFd = openDetachedDescriptor(uri)
            val validation = if (validationFd == null) {
                1
            } else {
                RomImportBridge.nativeValidateRom(validationFd, size)
            }
            val result = if (validation == 0) {
                val importFd = openDetachedDescriptor(uri)
                if (importFd == null) {
                    1
                } else {
                    RomImportBridge.nativeImportRom(
                        importFd,
                        size,
                        filesDir.resolve("ykw2-cache").absolutePath,
                    )
                }
            } else {
                validation
            }
            runOnUiThread {
                selectButton.isEnabled = rightsCheck.isChecked
                status.text = when (result) {
                    0 -> {
                        refreshImportState()
                        "Importación completada. Recursos guardados en el caché privado."
                    }
                    1 -> "La imagen seleccionada no se puede leer."
                    2 -> "La imagen no es un dump NCSD/3DS válido."
                    else -> "No se pudo extraer la RomFS; revisa que el dump esté descifrado y completo."
                }
            }
        }
    }

    private fun refreshImportState() {
        if (!RomImportBridge.available) return
        when (RomImportBridge.nativeCheckImport(filesDir.resolve("ykw2-cache").absolutePath)) {
            0 -> {
                runtimeButton.visibility = View.VISIBLE
                status.text = "Recursos importados y verificados."
            }
            2 -> {
                runtimeButton.visibility = View.GONE
                status.text = "La caché existe, pero la importación está incompleta; selecciona la ROM de nuevo."
            }
            else -> runtimeButton.visibility = View.GONE
        }
    }

    private fun openDetachedDescriptor(uri: Uri): Int? {
        val descriptor = contentResolver.openFileDescriptor(uri, "r") ?: return null
        return descriptor.detachFd()
    }

    private fun querySize(uri: Uri): Long {
        val cursor: Cursor = contentResolver.query(
            uri,
            arrayOf(OpenableColumns.SIZE),
            null,
            null,
            null,
        ) ?: return -1L
        cursor.use {
            if (!it.moveToFirst()) return -1L
            return it.getLong(0)
        }
    }

    companion object {
        private const val REQUEST_ROM = 1001
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
    external fun nativeRuntimeBackgroundWidth(): Int
    external fun nativeRuntimeBackgroundHeight(): Int
    external fun nativeRuntimeBackgroundRgba(): ByteArray?
}
