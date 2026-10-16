package com.ykw2.port

import android.media.AudioTrack
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

/** Keeps blocking AudioTrack writes off the emulation/render thread. */
internal class AudioOutput(private val track: AudioTrack) {
    internal class Block {
        val samples = ShortArray(FRAMES_PER_BLOCK * CHANNELS)
        var frames = 0
    }

    private val freeBlocks = ArrayBlockingQueue<Block>(POOL_SIZE)
    private val readyBlocks = ArrayBlockingQueue<Block>(POOL_SIZE)
    @Volatile private var running = true
    private val writer = thread(name = "zakuro-audio-output", isDaemon = true) {
        while (running) {
            val block = try {
                readyBlocks.poll(100, TimeUnit.MILLISECONDS)
            } catch (_: InterruptedException) {
                null
            } ?: continue
            var offset = 0
            val sampleCount = block.frames * CHANNELS
            while (running && offset < sampleCount) {
                val written = track.write(block.samples, offset, sampleCount - offset, AudioTrack.WRITE_BLOCKING)
                if (written <= 0) break
                offset += written
            }
            block.frames = 0
            freeBlocks.offer(block)
        }
    }

    init {
        repeat(POOL_SIZE) { freeBlocks.offer(Block()) }
    }

    fun obtain(): Block? = freeBlocks.poll()

    fun submit(block: Block, frames: Int) {
        if (frames <= 0) {
            recycle(block)
            return
        }
        block.frames = frames.coerceAtMost(FRAMES_PER_BLOCK)
        if (!readyBlocks.offer(block)) {
            readyBlocks.poll()?.let(::recycle)
            if (!readyBlocks.offer(block)) recycle(block)
        }
    }

    fun recycle(block: Block) {
        block.frames = 0
        freeBlocks.offer(block)
    }

    fun play() = track.play()

    fun pause() {
        track.pause()
        track.flush()
        var block = readyBlocks.poll()
        while (block != null) {
            recycle(block)
            block = readyBlocks.poll()
        }
    }

    fun close() {
        running = false
        track.pause()
        track.flush()
        writer.interrupt()
        track.release()
        try {
            writer.join(500)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }

    companion object {
        private const val FRAMES_PER_BLOCK = 2048
        private const val CHANNELS = 2
        private const val POOL_SIZE = 6
    }
}
