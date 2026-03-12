/* video.c — Simple framebuffer video controller */
#include "devices/video.h"
#include <stdlib.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────── */

static inline u32 rgb565ToRGBA(u16 c) {
    u32 r = ((c >> 11) & 0x1F) << 3;
    u32 g = ((c >>  5) & 0x3F) << 2;
    u32 b = ((c >>  0) & 0x1F) << 3;
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

/* ── register access ─────────────────────────────────── */

static u8 videoReadByte(Device *dev, u32 off) {
    VideoState *s = (VideoState *)dev->data;
    if (off >= VIDEO_VRAM && off < VIDEO_VRAM + VIDEO_VRAM_MAX)
        return s->vram[off - VIDEO_VRAM];
    if (off >= VIDEO_PALETTE && off < VIDEO_PALETTE + VIDEO_PALETTE_ENTRIES * 2) {
        u32 idx = (off - VIDEO_PALETTE) / 2;
        return ((off - VIDEO_PALETTE) & 1) ? (u8)(s->palette[idx]) : (u8)(s->palette[idx] >> 8);
    }
    switch (off) {
        case VIDEO_CTRL:     return (u8)(s->ctrl >> 8);
        case VIDEO_CTRL+1:   return (u8)s->ctrl;
        case VIDEO_STATUS:   return (u8)(s->status >> 8);
        case VIDEO_STATUS+1: return (u8)s->status;
        case VIDEO_WIDTH:    return (u8)(s->width >> 8);
        case VIDEO_WIDTH+1:  return (u8)s->width;
        case VIDEO_HEIGHT:   return (u8)(s->height >> 8);
        case VIDEO_HEIGHT+1: return (u8)s->height;
        default: return 0;
    }
}

static u16 videoReadWord(Device *dev, u32 off) {
    VideoState *s = (VideoState *)dev->data;
    if (off >= VIDEO_VRAM && off + 1 < VIDEO_VRAM + VIDEO_VRAM_MAX)
        return ((u16)s->vram[off - VIDEO_VRAM] << 8) | s->vram[off - VIDEO_VRAM + 1];
    if (off >= VIDEO_PALETTE && off < VIDEO_PALETTE + VIDEO_PALETTE_ENTRIES * 2) {
        u32 idx = (off - VIDEO_PALETTE) / 2;
        return s->palette[idx];
    }
    switch (off) {
        case VIDEO_CTRL:   return s->ctrl;
        case VIDEO_STATUS: return s->status;
        case VIDEO_WIDTH:  return s->width;
        case VIDEO_HEIGHT: return s->height;
        default: return 0;
    }
}

static void videoWriteByte(Device *dev, u32 off, u8 val) {
    VideoState *s = (VideoState *)dev->data;
    if (off >= VIDEO_VRAM && off < VIDEO_VRAM + VIDEO_VRAM_MAX) {
        s->vram[off - VIDEO_VRAM] = val;
        return;
    }
    if (off >= VIDEO_PALETTE && off < VIDEO_PALETTE + VIDEO_PALETTE_ENTRIES * 2) {
        u32 idx = (off - VIDEO_PALETTE) / 2;
        if ((off - VIDEO_PALETTE) & 1)
            s->palette[idx] = (s->palette[idx] & 0xFF00) | val;
        else
            s->palette[idx] = (s->palette[idx] & 0x00FF) | ((u16)val << 8);
        return;
    }
    if (off == VIDEO_CTRL + 1) { s->ctrl = (s->ctrl & 0xFF00) | val; return; }
    if (off == VIDEO_CTRL)     { s->ctrl = (s->ctrl & 0x00FF) | ((u16)val << 8); return; }
    /* STATUS is read-only; writes to WIDTH/HEIGHT are ignored at runtime */
}

static void videoWriteWord(Device *dev, u32 off, u16 val) {
    VideoState *s = (VideoState *)dev->data;
    if (off >= VIDEO_VRAM && off + 1 < VIDEO_VRAM + VIDEO_VRAM_MAX) {
        s->vram[off - VIDEO_VRAM]     = (u8)(val >> 8);
        s->vram[off - VIDEO_VRAM + 1] = (u8)val;
        return;
    }
    if (off >= VIDEO_PALETTE && off < VIDEO_PALETTE + VIDEO_PALETTE_ENTRIES * 2) {
        u32 idx = (off - VIDEO_PALETTE) / 2;
        s->palette[idx] = val;
        return;
    }
    if (off == VIDEO_CTRL) { s->ctrl = val; return; }
    if (off == VIDEO_STATUS) { s->status &= ~val; return; } /* write 1 to clear */
}

/* ── tick: accumulate cycles, trigger vblank ────────── */

static void videoTick(Device *dev, u32 cycles) {
    VideoState *s = (VideoState *)dev->data;
    if (!(s->ctrl & 1)) return;                    /* display disabled */
    s->cycleAccum += cycles;
    if (s->cycleAccum >= s->cyclesPerFrame) {
        s->cycleAccum -= s->cyclesPerFrame;
        s->status |= 1;                            /* vblank pending  */
        s->vblank = true;
    }
}

static int videoGetIRQ(Device *dev) {
    VideoState *s = (VideoState *)dev->data;
    return (s->status & 1) && (s->ctrl & 2) ? s->interruptLevel : 0;
}

static void videoReset(Device *dev) {
    VideoState *s = (VideoState *)dev->data;
    s->ctrl   = 0;
    s->status = 0;
    s->width  = VIDEO_DEFAULT_W;
    s->height = VIDEO_DEFAULT_H;
    s->cycleAccum = 0;
    s->vblank = false;
    memset(s->vram, 0, VIDEO_VRAM_MAX);
    /* leave palette untouched so ROM-initialised palettes survive soft reset */
}

static void videoDestroy(Device *dev) { free(dev->data); free(dev); }

/* ── constructor ─────────────────────────────────────── */

Device *videoCreate(u32 baseAddress, int interruptLevel, u32 cpuClockHz, u32 refreshHz) {
    Device *dev    = calloc(1, sizeof(Device));
    VideoState *s  = calloc(1, sizeof(VideoState));
    s->width          = VIDEO_DEFAULT_W;
    s->height         = VIDEO_DEFAULT_H;
    s->interruptLevel = interruptLevel;
    s->cyclesPerFrame = cpuClockHz / (refreshHz ? refreshHz : 60);

    dev->name      = "VIDEO";
    dev->baseAddress = baseAddress;
    dev->size      = VIDEO_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = videoReadByte;
    dev->readWord  = videoReadWord;
    dev->writeByte = videoWriteByte;
    dev->writeWord = videoWriteWord;
    dev->reset     = videoReset;
    dev->destroy   = videoDestroy;
    dev->getInterruptLevel = videoGetIRQ;
    dev->tick      = videoTick;
    return dev;
}

/* ── host rendering helper ───────────────────────────── */

void videoRenderRGBA(Device *dev, u32 *rgba, int bufW, int bufH) {
    VideoState *s = (VideoState *)dev->data;
    int w = (int)s->width;
    int h = (int)s->height;
    for (int y = 0; y < h && y < bufH; y++) {
        for (int x = 0; x < w && x < bufW; x++) {
            u8 idx = s->vram[y * w + x];
            rgba[y * bufW + x] = rgb565ToRGBA(s->palette[idx]);
        }
    }
}
