/* audio.c — Simple audio tone generator */
#include "devices/audio.h"
#include <stdlib.h>
#include <string.h>

/* ── register access ─────────────────────────────────── */

static u8 audioReadByte(Device *dev, u32 off) {
    AudioState *s = (AudioState *)dev->data;
    if (off == 0x0A) return (u8)(s->status >> 8);
    if (off == 0x0B) return (u8)s->status;
    return 0;
}

static u16 audioReadWord(Device *dev, u32 off) {
    AudioState *s = (AudioState *)dev->data;
    if (off == 0x0A) return s->status;
    return 0;
}

static void audioWriteByte(Device *dev, u32 off, u8 val) {
    (void)dev; (void)off; (void)val;
}

static void audioWriteWord(Device *dev, u32 off, u16 val) {
    AudioState *s = (AudioState *)dev->data;
    switch (off) {
        case 0x00: s->freq[0]   = val; break;
        case 0x02: s->volume[0] = val & 0xFF; break;
        case 0x04: s->freq[1]   = val; break;
        case 0x06: s->volume[1] = val & 0xFF; break;
        case 0x08: s->ctrl = val; break;
        case 0x0A: s->status &= ~val; break;   /* write-1-clear */
        default: break;
    }
}

/* ── tick: generate square-wave samples ──────────────── */

static void audioTick(Device *dev, u32 cycles) {
    AudioState *s = (AudioState *)dev->data;
    if (!s->ctrl) return;

    for (u32 c = 0; c < cycles; c++) {
        s32 mix = 0;
        for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
            if (!(s->ctrl & (1 << ch)) || s->freq[ch] == 0) continue;
            s->phase[ch]++;
            if (s->phase[ch] >= (u32)s->freq[ch] * 2)
                s->phase[ch] = 0;
            s32 sample = (s->phase[ch] < (u32)s->freq[ch]) ? (s32)s->volume[ch] : -(s32)s->volume[ch];
            mix += sample;
        }
        /* clamp and store */
        if (mix > 127)  mix = 127;
        if (mix < -128) mix = -128;
        s->sampleBuf[s->sampleWritePos % AUDIO_SAMPLE_BUFFER] = (s8)mix;
        s->sampleWritePos++;
    }
}

static int audioGetIRQ(Device *dev) {
    AudioState *s = (AudioState *)dev->data;
    return (s->status & 1) ? s->interruptLevel : 0;
}

static void audioReset(Device *dev) {
    AudioState *s = (AudioState *)dev->data;
    memset(s->freq, 0, sizeof(s->freq));
    memset(s->volume, 0, sizeof(s->volume));
    memset(s->phase, 0, sizeof(s->phase));
    s->ctrl = 0; s->status = 0;
    s->sampleWritePos = 0;
    memset(s->sampleBuf, 0, sizeof(s->sampleBuf));
}

static void audioDestroy(Device *dev) { free(dev->data); free(dev); }

Device *audioCreate(u32 baseAddress, int interruptLevel) {
    Device *dev   = calloc(1, sizeof(Device));
    AudioState *s = calloc(1, sizeof(AudioState));
    s->interruptLevel = interruptLevel;

    dev->name      = "AUDIO";
    dev->baseAddress = baseAddress;
    dev->size      = AUDIO_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = audioReadByte;
    dev->readWord  = audioReadWord;
    dev->writeByte = audioWriteByte;
    dev->writeWord = audioWriteWord;
    dev->reset     = audioReset;
    dev->destroy   = audioDestroy;
    dev->getInterruptLevel = audioGetIRQ;
    dev->tick      = audioTick;
    return dev;
}

u32 audioDrainSamples(Device *dev, s8 *out, u32 maxSamples) {
    AudioState *s = (AudioState *)dev->data;
    u32 avail = s->sampleWritePos;
    if (avail > AUDIO_SAMPLE_BUFFER) avail = AUDIO_SAMPLE_BUFFER;
    if (avail > maxSamples) avail = maxSamples;
    u32 start = (s->sampleWritePos - avail) % AUDIO_SAMPLE_BUFFER;
    for (u32 i = 0; i < avail; i++)
        out[i] = s->sampleBuf[(start + i) % AUDIO_SAMPLE_BUFFER];
    return avail;
}
