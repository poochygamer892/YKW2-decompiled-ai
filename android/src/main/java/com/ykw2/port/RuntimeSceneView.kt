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
    private var meshVertices: List<FloatArray> = emptyList()
    private var meshIndices: List<Int> = emptyList()
    private var menuItems: List<String> = emptyList()
    private var selectedIndex = 0
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
        meshVertices: List<FloatArray>,
        meshIndices: List<Int>,
        menuItems: List<String>,
        selectedIndex: Int,
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
        this.meshVertices = meshVertices
        this.meshIndices = meshIndices
        this.menuItems = menuItems
        this.selectedIndex = selectedIndex
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
            "title_menu" -> drawMenu(canvas)
            "world" -> drawWorld(canvas)
            "dialogue" -> drawDialogue(canvas)
            "battle" -> drawBattle(canvas)
            else -> drawMenu(canvas)
        }
    }

    private fun drawMenu(canvas: Canvas) {
        val panelTop = height * 0.58f
        paint.style = Paint.Style.FILL
        paint.color = Color.argb(225, 8, 14, 25)
        canvas.drawRoundRect(RectF(12f, panelTop, width - 12f, height - 12f), 16f, 16f, paint)
        paint.color = Color.WHITE
        paint.textSize = 18f
        canvas.drawText("Yo-kai Watch 2", 28f, panelTop + 34f, paint)
        paint.textSize = 16f
        menuItems.forEachIndexed { index, item ->
            val top = panelTop + 52f + index * 38f
            paint.color = if (index == selectedIndex) Color.rgb(50, 170, 215)
            else Color.argb(170, 220, 230, 240)
            canvas.drawRoundRect(RectF(22f, top - 22f, width - 22f, top + 8f), 8f, 8f, paint)
            paint.color = if (index == selectedIndex) Color.WHITE else Color.rgb(15, 25, 35)
            canvas.drawText(item, 34f, top, paint)
        }
    }

    private fun drawWorld(canvas: Canvas) {
        val top = height * 0.08f
        val bottom = height * 0.72f
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = 1f
        paint.color = Color.argb(75, 100, 180, 210)
        val cell = max(24f, width / 9f)
        if (meshVertices.isEmpty() || meshIndices.size < 3) {
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
        } else {
            val minX = meshVertices.minOf { it[0] }
            val maxX = meshVertices.maxOf { it[0] }
            val minZ = meshVertices.minOf { it[2] }
            val maxZ = meshVertices.maxOf { it[2] }
            val spanX = max(0.001f, maxX - minX)
            val spanZ = max(0.001f, maxZ - minZ)
            fun point(index: Int): Pair<Float, Float> {
                val vertex = meshVertices[index]
                return Pair((vertex[0] - minX) / spanX * width,
                    bottom - (vertex[2] - minZ) / spanZ * (bottom - top))
            }
            paint.color = Color.argb(155, 90, 225, 255)
            for (index in meshIndices.indices step 3) {
                if (index + 2 >= meshIndices.size) break
                val a = meshIndices[index]
                val b = meshIndices[index + 1]
                val c = meshIndices[index + 2]
                if (a !in meshVertices.indices || b !in meshVertices.indices ||
                    c !in meshVertices.indices) continue
                val pa = point(a)
                val pb = point(b)
                val pc = point(c)
                canvas.drawLine(pa.first, pa.second, pb.first, pb.second, paint)
                canvas.drawLine(pb.first, pb.second, pc.first, pc.second, paint)
                canvas.drawLine(pc.first, pc.second, pa.first, pa.second, paint)
            }
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
        canvas.drawText(if (speaker.isEmpty()) "Evento de apertura" else speaker,
            28f, panelTop + 30f, paint)
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
