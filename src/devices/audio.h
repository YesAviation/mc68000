/* audio.h — Simple audio tone generator */
#ifndef M68K_AUDIO_H
#define M68K_AUDIO_H

#include "devices/device.h"

/*
 * Dual-channel square-wave tone generator.
 *
 * Register map (word):
 *   +0x00  CH0_FREQ   (W)  — frequency divider (period = divider × 2)
 *   +0x02  CH0_VOL    (W)  — volume 0-255 (low byte)
 *   +0x04  CH1_FREQ   (W)
 *   +0x06  CH1_VOL    (W)
 *   +0x08  CTRL       (W)  — bit0 = ch0 enable, bit1 = ch1 enable
 *   +0x0A  STATUS     (R)  — bit0 = buffer-half IRQ pending
 */

#define AUDIO_CHANNELS       2
#define AUDIO_SAMPLE_BUFFER  1024   /* samples per channel */
#define AUDIO_DEVICE_SIZE    0x10

typedef struct {
    u16  freq[AUDIO_CHANNELS];
    u16  volume[AUDIO_CHANNELS];
    u16  ctrl;
    u16  status;
    /* internal phase accumulators */
    u32  phase[AUDIO_CHANNELS];
    /* output sample ring buffer (signed 8-bit mono mix) */
    s8   sampleBuf[AUDIO_SAMPLE_BUFFER];
    u32  sampleWritePos;
    int  interruptLevel;
} AudioState;

Device *audioCreate(u32 baseAddress, int interruptLevel);

/* Host pulls rendered samples (returns number of samples copied) */
u32 audioDrainSamples(Device *dev, s8 *out, u32 maxSamples);

#endif /* M68K_AUDIO_H */
