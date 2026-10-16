package com.ykw2.port

import android.app.Activity
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.graphics.Bitmap
import android.content.res.Configuration
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.security.MessageDigest
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

/** Android presentation and touch controls for the actual Zakuro 3DS runtime. */
class ZakuroRuntimeActivity : Activity() {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val frameLoopRunning = AtomicBoolean(false)
    private val buttonMask = AtomicInteger(0)
    private val circleX = AtomicInteger(0)
    private val circleY = AtomicInteger(0)
    private val touchX = AtomicInteger(-1)
    private val touchY = AtomicInteger(-1)
    @Volatile private var resumed = false
    @Volatile private var opened = false
    @Volatile private var audioOutput: AudioOutput? = null
    private lateinit var status: TextView
    private lateinit var display: ZakuroScreenView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterImmersiveMode()
        val padding = (resources.displayMetrics.density * 12).toInt()
        val root = FrameLayout(this).apply {
            setBackgroundColor(android.graphics.Color.BLACK)
        }
        status = TextView(this).apply {
            text = "Abriendo la ROM con Zakuro…"
            gravity = Gravity.CENTER
            setTextColor(android.graphics.Color.WHITE)
            setPadding(padding, padding, padding, padding)
            setBackgroundColor(0xaa000000.toInt())
        }
        val screenView = ZakuroScreenView(this) { buttons, padX, padY, x, y ->
            buttonMask.set(buttons)
            circleX.set(padX)
            circleY.set(padY)
            touchX.set(x)
            touchY.set(y)
        }
        display = screenView
        root.addView(display, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT,
        ))
        root.addView(status, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP,
        ))
        setContentView(root)
        openSelectedRom()
    }

    private fun openSelectedRom() {
        val uriText = intent.getStringExtra(EXTRA_ROM_URI)
        if (uriText == null) {
            status.text = "Falta la ROM. Vuelve al importador y selecciónala de nuevo."
            return
        }
        thread(name = "zakuro-open-rom") {
            val result = try {
                val uri = android.net.Uri.parse(uriText)
                val romDirectory = File(filesDir, "ykw2-data").apply { mkdirs() }
                val romKey = MessageDigest.getInstance("SHA-256")
                    .digest(uriText.toByteArray(Charsets.UTF_8))
                    .joinToString("") { "%02x".format(it) }
                val romFile = File(romDirectory, "authorized-$romKey.3ds")
                if (!romFile.isFile || romFile.length() == 0L) {
                    val temporaryFile = File(romDirectory, "authorized-$romKey.3ds.part")
                    mainHandler.post { status.text = "Copiando la ROM al almacenamiento privado…" }
                    try {
                        val input = contentResolver.openInputStream(uri)
                            ?: error("El proveedor no pudo leer la ROM seleccionada.")
                        input.use { source ->
                            FileOutputStream(temporaryFile).buffered().use { destination ->
                                source.copyTo(destination, 1024 * 1024)
                            }
                        }
                        if (temporaryFile.length() == 0L) error("La ROM seleccionada está vacía.")
                        Files.move(
                            temporaryFile.toPath(),
                            romFile.toPath(),
                            StandardCopyOption.REPLACE_EXISTING,
                        )
                    } finally {
                        temporaryFile.delete()
                    }
                }
                val recompiled = filesDir.resolve("ykw2-cache/00040000001B2900.so")
                    .takeIf { it.isFile }?.absolutePath
                RomImportBridge.nativeEmulatorOpen(
                    romFile.absolutePath,
                    recompiled,
                    romDirectory.absolutePath,
                )
            } catch (exception: Exception) {
                exception.message ?: "No se pudo abrir la ROM."
            }
            mainHandler.post {
                if (result.isEmpty()) {
                    opened = true
                    audioOutput = AudioOutput(createAudioTrack()).also { it.play() }
                    status.visibility = View.GONE
                    if (resumed) startFrameLoop()
                } else {
                    status.text = "Error al iniciar Zakuro: $result"
                }
            }
        }
    }

    private fun startFrameLoop() {
        if (!opened || !resumed || !frameLoopRunning.compareAndSet(false, true)) return
        thread(name = "zakuro-frame-loop", isDaemon = true) {
            try {
            val audioDiscard = ShortArray(AUDIO_FRAMES_PER_READ * 2)
            val topBitmap = Bitmap.createBitmap(TOP_WIDTH, SCREEN_HEIGHT, Bitmap.Config.ARGB_8888)
            val bottomBitmap = Bitmap.createBitmap(BOTTOM_WIDTH, SCREEN_HEIGHT, Bitmap.Config.ARGB_8888)
            display.showScreens(topBitmap, bottomBitmap)
            var lastTouchX = -1
            var lastTouchY = -1
            while (resumed) {
                val startedAt = System.nanoTime()
                val x = touchX.get()
                val y = touchY.get()
                RomImportBridge.nativeEmulatorSetInput(
                    buttonMask.get(), circleX.get() / AXIS_SCALE, circleY.get() / AXIS_SCALE, x, y,
                )
                // Render at 30 Hz while advancing two emulated frames per tick. This
                // cuts screen copies, bitmap uploads and UI invalidations in half.
                val outcome = RomImportBridge.nativeEmulatorRunFrames(2)
                if (outcome != 2) {
                    mainHandler.post { status.text = "Zakuro se detuvo (estado $outcome)." }
                    resumed = false
                    break
                }
                if (RomImportBridge.nativeEmulatorRenderScreens(topBitmap, bottomBitmap) != 0) {
                    mainHandler.post { status.text = "No se pudieron leer las pantallas de Zakuro." }
                    resumed = false
                    break
                }
                val output = audioOutput
                val audioBlock = output?.obtain()
                val audioFrames = RomImportBridge.nativeEmulatorTakeAudio(audioBlock?.samples ?: audioDiscard)
                if (audioBlock != null) output?.submit(audioBlock, audioFrames)
                if (x != lastTouchX || y != lastTouchY) {
                    lastTouchX = x
                    lastTouchY = y
                    if (x < 0 || y < 0) {
                        touchX.set(-1)
                        touchY.set(-1)
                    }
                }
                display.postInvalidateOnAnimation()
                val elapsedMs = (System.nanoTime() - startedAt) / 1_000_000
                val delay = 33 - elapsedMs
                if (delay > 0) Thread.sleep(delay)
            }
            } finally {
                frameLoopRunning.set(false)
                if (resumed && opened) startFrameLoop()
            }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        enterImmersiveMode()
        display.requestLayout()
        display.invalidate()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersiveMode()
    }

    @Suppress("DEPRECATION")
    private fun enterImmersiveMode() {
        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)
        window.statusBarColor = android.graphics.Color.BLACK
        window.navigationBarColor = android.graphics.Color.BLACK
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            )
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        audioOutput?.play()
        if (opened) startFrameLoop()
    }

    override fun onPause() {
        resumed = false
        audioOutput?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        resumed = false
        audioOutput?.close()
        audioOutput = null
        if (opened) thread(name = "zakuro-close") { RomImportBridge.nativeEmulatorClose() }
        super.onDestroy()
    }

    companion object {
        const val EXTRA_ROM_URI = "ykw2.rom_uri"
        private const val TOP_WIDTH = 400
        private const val BOTTOM_WIDTH = 320
        private const val SCREEN_HEIGHT = 240
        private const val AUDIO_SAMPLE_RATE = 32_728
        private const val AUDIO_FRAMES_PER_READ = 2048
        private const val AXIS_SCALE = 1000f
    }

    private fun createAudioTrack(): AudioTrack {
        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(AUDIO_SAMPLE_RATE)
            .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
            .build()
        val minimum = AudioTrack.getMinBufferSize(
            AUDIO_SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_16BIT,
        ).let { maxOf(it * 2, AUDIO_FRAMES_PER_READ * 4) }
        return AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAudioFormat(format)
            .setBufferSizeInBytes(minimum)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
    }
}

internal class ZakuroScreenView(
    context: android.content.Context,
    private val onInput: (buttons: Int, circleX: Int, circleY: Int, touchX: Int, touchY: Int) -> Unit,
) : View(context) {
    @Volatile private var top: Bitmap? = null
    @Volatile private var bottom: Bitmap? = null
    private val controlPaint = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG)
    private val edgePaint = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        style = android.graphics.Paint.Style.STROKE
    }
    private val labelPaint = android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG).apply {
        color = android.graphics.Color.WHITE
        textAlign = android.graphics.Paint.Align.CENTER
        typeface = android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.BOLD)
    }
    private var activeButtons = 0
    private var stickX = 0f
    private var stickY = 0f
    private var stickPointerId = INVALID_POINTER
    private var controlLayout = TouchControlLayout.load(context, 0, 0)
    var editorMode = false
    var onEditorSelectionChanged: ((String) -> Unit)? = null
    private var editorPointerId = INVALID_POINTER
    private var editorGroup = EDIT_NONE
    private var selectedControl = TouchControlLayout.ALL
    private var editorLastX = 0f
    private var editorLastY = 0f

    fun showScreens(topScreen: Bitmap, bottomScreen: Bitmap) {
        top = topScreen
        bottom = bottomScreen
        postInvalidateOnAnimation()
    }

    fun selectedLayout(): TouchControlLayout = controlLayout

    fun selectControl(key: String) {
        selectedControl = key
        invalidate()
    }

    fun selectedControlScale(): Float = if (selectedControl == TouchControlLayout.ALL) {
        controlLayout.scale
    } else controlLayout.size(selectedControl)

    fun setSelectedControlScale(scale: Float) {
        if (selectedControl == TouchControlLayout.ALL) {
            controlLayout.scale = scale.coerceIn(TouchControlLayout.MIN_SIZE, TouchControlLayout.MAX_SIZE)
        } else controlLayout.setSize(selectedControl, scale)
        controlLayout.save(context)
        invalidate()
    }

    fun resetLayout() {
        controlLayout = TouchControlLayout.defaults(width, height)
        controlLayout.save(context)
        invalidate()
    }

    fun setControlScale(scale: Float) {
        controlLayout.scale = scale.coerceIn(TouchControlLayout.MIN_SIZE, TouchControlLayout.MAX_SIZE)
        controlLayout.save(context)
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        if (w > 0 && h > 0) {
            controlLayout = TouchControlLayout.load(context, w, h)
            invalidate()
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (editorMode) return handleEditorTouch(event)
        if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
            activeButtons = 0
            stickX = 0f
            stickY = 0f
            stickPointerId = INVALID_POINTER
            onInput(0, 0, 0, -1, -1)
            invalidate()
            return true
        }
        var buttons = 0
        var padX = 0f
        var padY = 0f
        var visualPadY = 0f
        var touchX = -1
        var touchY = -1
        val excluded = if (event.actionMasked == MotionEvent.ACTION_UP ||
            event.actionMasked == MotionEvent.ACTION_POINTER_UP) event.actionIndex else -1
        for (index in 0 until event.pointerCount) {
            if (index == excluded) continue
            val x = event.getX(index)
            val y = event.getY(index)
            val pointerId = event.getPointerId(index)
            if (pointerId == stickPointerId || controlAt(x, y) == STICK) {
                if (event.actionMasked == MotionEvent.ACTION_DOWN ||
                    event.actionMasked == MotionEvent.ACTION_POINTER_DOWN) stickPointerId = pointerId
                if (pointerId == stickPointerId) {
                    val unit = controlUnit()
                    val stickScale = controlLayout.size("STICK")
                    val dx = (x - width * controlLayout.stickX) / (unit * 2.25f * stickScale)
                    val dy = (y - height * controlLayout.stickY) / (unit * 2.25f * stickScale)
                    val magnitude = kotlin.math.sqrt(dx * dx + dy * dy).coerceAtLeast(1f)
                    padX = (dx / magnitude).coerceIn(-1f, 1f)
                    // Keep the thumb following the finger; Zakuro's game input expects
                    // the opposite vertical sign from Android's downward-positive screen axis.
                    visualPadY = (dy / magnitude).coerceIn(-1f, 1f)
                    padY = -visualPadY
                }
            } else {
                val key = controlAt(x, y)
                if (key != null) buttons = buttons or key
                else if (touchX < 0) mapBottomTouch(x, y)?.let { point ->
                    touchX = point.first
                    touchY = point.second
                }
            }
        }
        if (excluded >= 0 && event.getPointerId(excluded) == stickPointerId) stickPointerId = INVALID_POINTER
        activeButtons = buttons
        stickX = padX
        stickY = visualPadY
        onInput(buttons, (padX * 1000).toInt(), (padY * 1000).toInt(), touchX, touchY)
        invalidate()
        return true
    }

    private fun handleEditorTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                if (editorPointerId == INVALID_POINTER) {
                    val index = event.actionIndex
                    editorPointerId = event.getPointerId(index)
                    val x = event.getX(index)
                    val y = event.getY(index)
                    editorGroup = editorGroupAt(x, y)
                    controlKeyAt(x, y)?.let { key ->
                        selectedControl = key
                        onEditorSelectionChanged?.invoke(key)
                    }
                    editorLastX = x
                    editorLastY = y
                }
            }
            MotionEvent.ACTION_MOVE -> {
                val index = (0 until event.pointerCount).firstOrNull {
                    event.getPointerId(it) == editorPointerId
                } ?: return true
                val x = event.getX(index)
                val y = event.getY(index)
                moveEditorGroup((x - editorLastX) / width.coerceAtLeast(1), (y - editorLastY) / height.coerceAtLeast(1))
                editorLastX = x
                editorLastY = y
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                if (event.actionMasked == MotionEvent.ACTION_CANCEL ||
                    (event.pointerCount > 0 && event.getPointerId(event.actionIndex) == editorPointerId)) {
                    controlLayout.save(context)
                    editorPointerId = INVALID_POINTER
                    editorGroup = EDIT_NONE
                }
            }
        }
        return true
    }

    private fun editorGroupAt(x: Float, y: Float): Int {
        val u = controlUnit()
        fun near(px: Float, py: Float, radius: Float) =
            (x - px) * (x - px) + (y - py) * (y - py) <= radius * radius
        if (near(width * controlLayout.leftX, height * controlLayout.dpadY, u * 2.5f)) return EDIT_DPAD
        if (near(width * controlLayout.stickX, height * controlLayout.stickY, u * 2.6f)) return EDIT_STICK
        if (near(width * controlLayout.faceX, height * controlLayout.faceY, u * 3.0f)) return EDIT_FACE
        if (near(width * controlLayout.utilityX, height * controlLayout.utilityY, u * 2.4f)) return EDIT_UTILITY
        if (near(width * controlLayout.leftShoulderX, height * controlLayout.leftShoulderY, u * 1.5f)) return EDIT_LEFT_SHOULDER
        if (near(width * controlLayout.rightShoulderX, height * controlLayout.rightShoulderY, u * 1.5f)) return EDIT_RIGHT_SHOULDER
        return EDIT_NONE
    }

    private fun controlKeyAt(x: Float, y: Float): String? {
        val u = controlUnit()
        fun near(cx: Float, cy: Float, radius: Float) =
            (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius
        val left = width * controlLayout.leftX
        val dpadY = height * controlLayout.dpadY
        val directions = listOf(
            Triple("UP", left, dpadY - u * 1.05f),
            Triple("DOWN", left, dpadY + u * 1.05f),
            Triple("LEFT", left - u * 1.05f, dpadY),
            Triple("RIGHT", left + u * 1.05f, dpadY),
        )
        directions.firstOrNull { (key, cx, cy) -> near(cx, cy, u * 0.6f * controlLayout.size(key)) }
            ?.let { return it.first }
        if (near(width * controlLayout.stickX, height * controlLayout.stickY, u * 2.25f * controlLayout.size("STICK"))) return "STICK"
        val right = width * controlLayout.faceX
        val faceY = height * controlLayout.faceY
        val faces = listOf(
            Triple("X", right, faceY - u * 1.35f),
            Triple("Y", right - u * 1.35f, faceY),
            Triple("A", right + u * 1.35f, faceY),
            Triple("B", right, faceY + u * 1.35f),
        )
        faces.firstOrNull { (key, cx, cy) -> near(cx, cy, u * 0.9f * controlLayout.size(key)) }
            ?.let { return it.first }
        val utilities = listOf(
            Triple("SELECT", width * (controlLayout.utilityX - 0.08f), height * controlLayout.utilityY),
            Triple("START", width * (controlLayout.utilityX + 0.08f), height * controlLayout.utilityY),
            Triple("L", width * controlLayout.leftShoulderX, height * controlLayout.leftShoulderY),
            Triple("R", width * controlLayout.rightShoulderX, height * controlLayout.rightShoulderY),
        )
        utilities.firstOrNull { (key, cx, cy) -> near(cx, cy, u * 0.95f * controlLayout.size(key)) }
            ?.let { return it.first }
        return null
    }

    private fun moveEditorGroup(dx: Float, dy: Float) {
        when (editorGroup) {
            EDIT_DPAD -> {
                controlLayout.leftX = (controlLayout.leftX + dx).coerceIn(0.04f, 0.96f)
                controlLayout.dpadY = (controlLayout.dpadY + dy).coerceIn(0.05f, 0.95f)
            }
            EDIT_STICK -> {
                controlLayout.stickX = (controlLayout.stickX + dx).coerceIn(0.04f, 0.96f)
                controlLayout.stickY = (controlLayout.stickY + dy).coerceIn(0.05f, 0.95f)
            }
            EDIT_FACE -> {
                controlLayout.faceX = (controlLayout.faceX + dx).coerceIn(0.04f, 0.96f)
                controlLayout.faceY = (controlLayout.faceY + dy).coerceIn(0.05f, 0.95f)
            }
            EDIT_UTILITY -> {
                controlLayout.utilityX = (controlLayout.utilityX + dx).coerceIn(0.08f, 0.92f)
                controlLayout.utilityY = (controlLayout.utilityY + dy).coerceIn(0.05f, 0.95f)
            }
            EDIT_LEFT_SHOULDER -> {
                controlLayout.leftShoulderX = (controlLayout.leftShoulderX + dx).coerceIn(0.04f, 0.96f)
                controlLayout.leftShoulderY = (controlLayout.leftShoulderY + dy).coerceIn(0.05f, 0.95f)
            }
            EDIT_RIGHT_SHOULDER -> {
                controlLayout.rightShoulderX = (controlLayout.rightShoulderX + dx).coerceIn(0.04f, 0.96f)
                controlLayout.rightShoulderY = (controlLayout.rightShoulderY + dy).coerceIn(0.05f, 0.95f)
            }
        }
    }

    private fun controlUnit(): Float = (minOf(width * 0.065f, height * 0.105f) * controlLayout.scale).coerceAtLeast(14f)

    private fun controlAt(x: Float, y: Float): Int? {
        val u = controlUnit()
        fun near(cx: Float, cy: Float, radius: Float) =
            (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius

        val left = width * controlLayout.leftX
        val dpadY = height * controlLayout.dpadY
        if (near(left, dpadY - u * 1.05f, u * 0.6f * controlLayout.size("UP"))) return 1 shl 6
        if (near(left, dpadY + u * 1.05f, u * 0.6f * controlLayout.size("DOWN"))) return 1 shl 7
        if (near(left - u * 1.05f, dpadY, u * 0.6f * controlLayout.size("LEFT"))) return 1 shl 5
        if (near(left + u * 1.05f, dpadY, u * 0.6f * controlLayout.size("RIGHT"))) return 1 shl 4
        if (near(width * controlLayout.stickX, height * controlLayout.stickY, u * 2.25f * controlLayout.size("STICK"))) return STICK

        val right = width * controlLayout.faceX
        val faceY = height * controlLayout.faceY
        if (near(right, faceY - u * 1.35f, u * 0.95f * controlLayout.size("X"))) return 1 shl 10
        if (near(right - u * 1.35f, faceY, u * 0.95f * controlLayout.size("Y"))) return 1 shl 11
        if (near(right + u * 1.35f, faceY, u * 0.95f * controlLayout.size("A"))) return 1 shl 0
        if (near(right, faceY + u * 1.35f, u * 0.95f * controlLayout.size("B"))) return 1 shl 1

        if (near(width * (controlLayout.utilityX - 0.08f), height * controlLayout.utilityY, u * 0.8f * controlLayout.size("SELECT"))) return 1 shl 2
        if (near(width * (controlLayout.utilityX + 0.08f), height * controlLayout.utilityY, u * 0.8f * controlLayout.size("START"))) return 1 shl 3
        if (near(width * controlLayout.leftShoulderX, height * controlLayout.leftShoulderY, u * 1.1f * controlLayout.size("L"))) return 1 shl 9
        if (near(width * controlLayout.rightShoulderX, height * controlLayout.rightShoulderY, u * 1.1f * controlLayout.size("R"))) return 1 shl 8
        return null
    }

    fun mapBottomTouch(x: Float, y: Float): Pair<Int, Int>? {
        val scale = minOf(width / 400f, height / 480f)
        if (scale <= 0f) return null
        val left = (width - 400f * scale) / 2f
        val topY = (height - 480f * scale) / 2f + 240f * scale
        val bottomLeft = left + 40f * scale
        val bottomX = ((x - bottomLeft) / scale).toInt()
        val bottomY = ((y - topY) / scale).toInt()
        return if (bottomX in 0 until 320 && bottomY in 0 until 240) bottomX to bottomY else null
    }

    override fun onDraw(canvas: android.graphics.Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(android.graphics.Color.BLACK)
        val scale = minOf(width / 400f, height / 480f)
        if (scale <= 0f) return
        val totalHeight = 480f * scale
        val left = (width - 400f * scale) / 2f
        val topY = (height - totalHeight) / 2f
        top?.let {
            canvas.drawBitmap(it, null, android.graphics.RectF(
                left, topY, left + 400f * scale, topY + 240f * scale,
            ), null)
        }
        bottom?.let {
            val bottomLeft = left + 40f * scale
            canvas.drawBitmap(it, null, android.graphics.RectF(
                bottomLeft, topY + 240f * scale,
                bottomLeft + 320f * scale, topY + 480f * scale,
            ), null)
        }
        drawTouchControls(canvas)
    }

    private fun drawTouchControls(canvas: android.graphics.Canvas) {
        val u = controlUnit()
        val left = width * controlLayout.leftX
        val dpadY = height * controlLayout.dpadY
        val right = width * controlLayout.faceX
        val faceY = height * controlLayout.faceY
        drawDpad(canvas, left, dpadY, u)
        drawStick(canvas, width * controlLayout.stickX, height * controlLayout.stickY, u)
        drawFaceButton(canvas, right, faceY - u * 1.35f, u * 0.72f * controlLayout.size("X"), "X", 1 shl 10)
        drawFaceButton(canvas, right - u * 1.35f, faceY, u * 0.72f * controlLayout.size("Y"), "Y", 1 shl 11)
        drawFaceButton(canvas, right + u * 1.35f, faceY, u * 0.72f * controlLayout.size("A"), "A", 1 shl 0)
        drawFaceButton(canvas, right, faceY + u * 1.35f, u * 0.72f * controlLayout.size("B"), "B", 1 shl 1)
        drawSmallButton(canvas, width * (controlLayout.utilityX - 0.08f), height * controlLayout.utilityY, u * 0.57f * controlLayout.size("SELECT"), "SEL", 1 shl 2)
        drawSmallButton(canvas, width * (controlLayout.utilityX + 0.08f), height * controlLayout.utilityY, u * 0.57f * controlLayout.size("START"), "START", 1 shl 3)
        drawSmallButton(canvas, width * controlLayout.leftShoulderX, height * controlLayout.leftShoulderY, u * 0.68f * controlLayout.size("L"), "L", 1 shl 9)
        drawSmallButton(canvas, width * controlLayout.rightShoulderX, height * controlLayout.rightShoulderY, u * 0.68f * controlLayout.size("R"), "R", 1 shl 8)
    }

    private fun drawDpad(canvas: android.graphics.Canvas, cx: Float, cy: Float, u: Float) {
        val arm = u * 0.82f
        val radius = u * 0.28f
        val rects = arrayOf(
            android.graphics.RectF(cx - arm * 0.48f, cy - arm * 1.55f, cx + arm * 0.48f, cy + arm * 1.55f),
            android.graphics.RectF(cx - arm * 1.55f, cy - arm * 0.48f, cx + arm * 1.55f, cy + arm * 0.48f),
        )
        controlPaint.color = 0x990a1019.toInt()
        rects.forEach { canvas.drawRoundRect(it, radius, radius, controlPaint) }
        edgePaint.color = 0x887f95a8.toInt()
        edgePaint.strokeWidth = (u * 0.09f).coerceAtLeast(1f)
        rects.forEach { canvas.drawRoundRect(it, radius, radius, edgePaint) }
        drawDpadDirection(canvas, cx, cy - u * 1.05f, u * 0.46f * controlLayout.size("UP"), 1 shl 6)
        drawDpadDirection(canvas, cx, cy + u * 1.05f, u * 0.46f * controlLayout.size("DOWN"), 1 shl 7)
        drawDpadDirection(canvas, cx - u * 1.05f, cy, u * 0.46f * controlLayout.size("LEFT"), 1 shl 5)
        drawDpadDirection(canvas, cx + u * 1.05f, cy, u * 0.46f * controlLayout.size("RIGHT"), 1 shl 4)
    }

    private fun drawStick(canvas: android.graphics.Canvas, cx: Float, cy: Float, u: Float) {
        val s = controlLayout.size("STICK")
        controlPaint.color = 0x66515d6b
        canvas.drawCircle(cx, cy, u * 2.25f * s, controlPaint)
        edgePaint.color = 0x995f7385.toInt()
        edgePaint.strokeWidth = (u * 0.12f).coerceAtLeast(1.5f)
        canvas.drawCircle(cx, cy, u * 2.25f * s, edgePaint)
        controlPaint.color = 0x99505d6a.toInt()
        val dx = stickX * u * 1.1f * s
        val dy = stickY * u * 1.1f * s
        canvas.drawCircle(cx + dx, cy + dy, u * 1.14f * s, controlPaint)
        edgePaint.color = 0x99d0dbe4.toInt()
        edgePaint.strokeWidth = (u * 0.1f).coerceAtLeast(1f)
        canvas.drawCircle(cx + dx, cy + dy, u * 1.14f * s, edgePaint)
    }

    private fun drawFaceButton(canvas: android.graphics.Canvas, cx: Float, cy: Float, radius: Float, label: String, bit: Int) {
        controlPaint.color = if (activeButtons and bit != 0) 0xcc39a9db.toInt() else 0x88738391.toInt()
        canvas.drawCircle(cx, cy, radius, controlPaint)
        edgePaint.color = 0x99e1e9f0.toInt()
        edgePaint.strokeWidth = (radius * 0.12f).coerceAtLeast(1f)
        canvas.drawCircle(cx, cy, radius, edgePaint)
        labelPaint.textSize = radius * 0.88f
        canvas.drawText(label, cx, cy + labelPaint.textSize * 0.34f, labelPaint)
    }

    private fun drawSmallButton(canvas: android.graphics.Canvas, cx: Float, cy: Float, radius: Float, label: String, bit: Int) {
        controlPaint.color = if (activeButtons and bit != 0) 0xcc39a9db.toInt() else 0x77535d67
        canvas.drawRoundRect(cx - radius * 1.3f, cy - radius * 0.62f, cx + radius * 1.3f, cy + radius * 0.62f, radius, radius, controlPaint)
        labelPaint.textSize = radius * 0.53f
        canvas.drawText(label, cx, cy + labelPaint.textSize * 0.34f, labelPaint)
    }

    private fun drawPressedGlow(canvas: android.graphics.Canvas, cx: Float, cy: Float, radius: Float, bit: Int) {
        if (activeButtons and bit == 0) return
        controlPaint.color = 0xaa39a9db.toInt()
        canvas.drawCircle(cx, cy, radius, controlPaint)
    }

    private fun drawDpadDirection(canvas: android.graphics.Canvas, cx: Float, cy: Float, radius: Float, bit: Int) {
        controlPaint.color = if (activeButtons and bit != 0) 0xcc39a9db.toInt() else 0x44515d6b
        canvas.drawCircle(cx, cy, radius, controlPaint)
        if (activeButtons and bit != 0) {
            edgePaint.color = 0x99e1e9f0.toInt()
            edgePaint.strokeWidth = (radius * 0.14f).coerceAtLeast(1f)
            canvas.drawCircle(cx, cy, radius, edgePaint)
        }
    }

    companion object {
        private const val STICK = Int.MIN_VALUE
        private const val INVALID_POINTER = -1
        private const val EDIT_NONE = 0
        private const val EDIT_DPAD = 1
        private const val EDIT_STICK = 2
        private const val EDIT_FACE = 3
        private const val EDIT_UTILITY = 4
        private const val EDIT_LEFT_SHOULDER = 5
        private const val EDIT_RIGHT_SHOULDER = 6
    }
}
