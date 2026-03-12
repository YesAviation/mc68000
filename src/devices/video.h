/* video.h — Simple framebuffer video controller */
#ifndef M68K_VIDEO_H
#define M68K_VIDEO_H

#include "devices/device.h"

/*
 * Minimal framebuffer device.
 *
 * Register map (relative to base):
 *   +0x0000  CTRL      (u16) — bit 0 = enable, bit 1 = vblank IRQ enable
 *   +0x0002  STATUS    (u16) — bit 0 = vblank pending
 *   +0x0004  WIDTH     (u16) — pixels per row
 *   +0x0006  HEIGHT    (u16) — rows
 *   +0x0008  PALETTE[0..255] — 256 × u16 (RGB565)
 *   +0x0208  reserved
 *   +0x1000  VRAM      — width × height bytes (8-bit indexed colour)
 *
 * Default resolution: 320 × 240.
 */

#define VIDEO_CTRL     0x0000
#define VIDEO_STATUS   0x0002
#define VIDEO_WIDTH    0x0004
#define VIDEO_HEIGHT   0x0006
#define VIDEO_PALETTE  0x0008
#define VIDEO_VRAM     0x1000

#define VIDEO_DEFAULT_W  320
#define VIDEO_DEFAULT_H  240
#define VIDEO_PALETTE_ENTRIES 256
#define VIDEO_VRAM_MAX   (320 * 240)  /* bytes */
#define VIDEO_DEVICE_SIZE (VIDEO_VRAM + VIDEO_VRAM_MAX)

typedef struct {
    u16  ctrl;
    u16  status;
    u16  width;
    u16  height;
    u16  palette[VIDEO_PALETTE_ENTRIES];
    u8   vram[VIDEO_VRAM_MAX];
    int  interruptLevel;
    u32  cycleAccum;          /* cycle accumulator for vblank timing */
    u32  cyclesPerFrame;      /* cycles between vblanks              */
    bool vblank;
} VideoState;

Device *videoCreate(u32 baseAddress, int interruptLevel, u32 cpuClockHz, u32 refreshHz);

/* Host-side: copy VRAM into RGBA32 buffer for display */
void videoRenderRGBA(Device *dev, u32 *rgba, int bufW, int bufH);

#endif /* M68K_VIDEO_H */
