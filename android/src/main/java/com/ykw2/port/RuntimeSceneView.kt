package com.ykw2.port

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.View
import kotlin.math.max
import kotlin.math.min

/**
 * Small native renderer for the portable runtime snapshot. It is deliberately
 * data-driven: the C++ scene owns state while this view owns Android drawing.
 */
class RuntimeSceneView(context: Context) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var background: Bitmap? = null
    private var screen = "title_menu"
    private var playerX = 0f
    private var playerY = 0f
    private var speaker = ""
    private var dialogueText = ""
    private var enemyId = ""
    private var playerHp = 0
    private var playerMaxHp = 1
    private var enemyHp = 0
    private var enemyMaxHp = 1
    private var sceneHeightPercent = 58f
    private var dialogueTopPercent = 55f

    fun render(
        screen: String,
        background: Bitmap?,
        playerX: Float,
        playerY: Float,
        speaker: String,
        dialogueText: String,
        enemyId: String,
        playerHp: Int,
        playerMaxHp: Int,
        enemyHp: Int,
        enemyMaxHp: Int,
        sceneHeightPercent: Float,
        dialogueTopPercent: Float,
    ) {
        this.screen = screen
        this.background = background
        this.playerX = playerX
        this.playerY = playerY
        this.speaker = speaker
        this.dialogueText = dialogueText
        this.enemyId = enemyId
        this.playerHp = playerHp
        this.playerMaxHp = max(1, playerMaxHp)
        this.enemyHp = enemyHp
        this.enemyMaxHp = max(1, enemyMaxHp)
        this.sceneHeightPercent = sceneHeightPercent.coerceIn(20f, 90f)
        this.dialogueTopPercent = dialogueTopPercent.coerceIn(30f, 90f)
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.rgb(7, 10, 15))
        val image = background
        if (image != null && !image.isRecycled) {
            val imageHeight = height * sceneHeightPercent / 100f
            val scale = min(width.toFloat() / image.width, imageHeight / image.height)
            val drawWidth = image.width * scale
            val drawHeight = image.height * scale
            val destination = RectF(
                (width - drawWidth) / 2f,
                0f,
                (width + drawWidth) / 2f,
                drawHeight,
            )
            paint.alpha = 255
            canvas.drawBitmap(image, null, destination, paint)
        }
        when (screen) {
            "world" -> drawWorld(canvas)
            "dialogue" -> drawDialogue(canvas)
            "battle" -> drawBattle(canvas)
            else -> drawMenuHint(canvas)
        }
    }

    private fun drawWorld(canvas: Canvas) {
        val top = height * 0.08f
        val bottom = height * 0.72f
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = 1f
        paint.color = Color.argb(75, 100, 180, 210)
        val cell = max(24f, width / 9f)
        var x = 0f
        while (x <= width) {
            canvas.drawLine(x, top, x, bottom, paint)
            x += cell
        }
        var y = top
        while (y <= bottom) {
            canvas.drawLine(0f, y, width.toFloat(), y, paint)
            y += cell
        }
        paint.style = Paint.Style.FILL
        paint.color = Color.rgb(67, 218, 190)
        val playerCenterX = width / 2f + playerX * 8f
        val playerCenterY = (top + bottom) / 2f + playerY * 8f
        canvas.drawCircle(playerCenterX, playerCenterY, 14f, paint)
        paint.color = Color.WHITE
        paint.textSize = 14f
        canvas.drawText("Exploración", 16f, bottom + 28f, paint)
    }

    private fun drawDialogue(canvas: Canvas) {
        val panelTop = height * dialogueTopPercent / 100f
        paint.style = Paint.Style.FILL
        paint.color = Color.argb(230, 8, 14, 25)
        canvas.drawRoundRect(RectF(12f, panelTop, width - 12f, height - 10f), 14f, 14f, paint)
        paint.color = Color.rgb(106, 220, 255)
        paint.textSize = 17f
        canvas.drawText(speaker, 28f, panelTop + 30f, paint)
        paint.color = Color.WHITE
        paint.textSize = 16f
        drawWrapped(canvas, dialogueText, 28f, panelTop + 62f, width - 56f, 23f)
    }

    private fun drawBattle(canvas: Canvas) {
        paint.style = Paint.Style.FILL
        paint.color = Color.argb(220, 16, 25, 45)
        canvas.drawRoundRect(RectF(12f, 12f, width - 12f, height - 12f), 16f, 16f, paint)
        paint.color = Color.WHITE
        paint.textSize = 17f
        canvas.drawText("Combate de Yo-kai Watch 2", 18f, 42f, paint)
        paint.textSize = 14f
        drawWrapped(canvas, "Las reglas, encuentros y comandos se cargarán desde el dump autorizado.", 18f, 76f, width - 36f, 22f)
    }

    private fun drawMenuHint(canvas: Canvas) {
        paint.style = Paint.Style.FILL
        paint.color = Color.WHITE
        paint.textSize = 15f
        canvas.drawText("Selecciona una opción", 16f, height * 0.72f, paint)
    }

    private fun drawWrapped(canvas: Canvas, text: String, left: Float, top: Float, maxWidth: Float, lineHeight: Float) {
        var line = ""
        var y = top
        for (word in text.split(" ")) {
            val candidate = if (line.isEmpty()) word else "$line $word"
            if (paint.measureText(candidate) > maxWidth && line.isNotEmpty()) {
                canvas.drawText(line, left, y, paint)
                y += lineHeight
                line = word
            } else {
                line = candidate
            }
        }
        if (line.isNotEmpty()) canvas.drawText(line, left, y, paint)
    }
}
