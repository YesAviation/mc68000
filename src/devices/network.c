/* network.c — Simple network interface */
#include "devices/network.h"
#include "bus/bus.h"
#include <stdlib.h>
#include <string.h>

/* ── register access ─────────────────────────────────── */

static u8 netReadByte(Device *dev, u32 off) {
    NetworkState *s = (NetworkState *)dev->data;
    if (off >= 0x0C && off < 0x12) return s->mac[off - 0x0C];
    switch (off) {
        case 0x02: return (u8)(s->status >> 8);
        case 0x03: return (u8)s->status;
        case 0x04: return (u8)(s->rxLen >> 8);
        case 0x05: return (u8)s->rxLen;
        default: return 0;
    }
}

static u16 netReadWord(Device *dev, u32 off) {
    NetworkState *s = (NetworkState *)dev->data;
    switch (off) {
        case 0x02: return s->status;
        case 0x04: return s->rxLen;
        default: return ((u16)netReadByte(dev, off) << 8) | netReadByte(dev, off + 1);
    }
}

static void netWriteByte(Device *dev, u32 off, u8 val) {
    NetworkState *s = (NetworkState *)dev->data;
    if (off >= 0x0C && off < 0x12) { s->mac[off - 0x0C] = val; return; }
}

static void netWriteWord(Device *dev, u32 off, u16 val) {
    NetworkState *s = (NetworkState *)dev->data;
    switch (off) {
        case 0x00:
            s->ctrl = val;
            if (val & 4) {
                /* TX start — DMA from bus into txBuf */
                if (s->bus && s->txLen <= NET_MAX_FRAME) {
                    for (u16 i = 0; i < s->txLen; i++)
                        s->txBuf[i] = busReadByte(s->bus, s->dmaAddress + i);
                    s->status |= 2;  /* tx done */
                }
                s->ctrl &= ~4;       /* auto-clear tx bit */
            }
            break;
        case 0x02: s->status &= ~val; break;  /* write-1-clear */
        case 0x06: s->txLen = val; break;
        case 0x08: s->dmaAddress = ((u32)val << 16) | (s->dmaAddress & 0xFFFF); break;
        case 0x0A: s->dmaAddress = (s->dmaAddress & 0xFFFF0000) | val; break;
        default:
            netWriteByte(dev, off, (u8)(val >> 8));
            netWriteByte(dev, off + 1, (u8)val);
            break;
    }
}

static int netGetIRQ(Device *dev) {
    NetworkState *s = (NetworkState *)dev->data;
    if (!(s->ctrl & 2)) return 0;
    return (s->status & 1) ? s->interruptLevel : 0;
}

static void netReset(Device *dev) {
    NetworkState *s = (NetworkState *)dev->data;
    s->ctrl = 0; s->status = 0;
    s->rxLen = 0; s->txLen = 0; s->dmaAddress = 0;
}

static void netDestroy(Device *dev) { free(dev->data); free(dev); }

Device *networkCreate(u32 baseAddress, int interruptLevel) {
    Device *dev    = calloc(1, sizeof(Device));
    NetworkState *s = calloc(1, sizeof(NetworkState));
    s->interruptLevel = interruptLevel;

    dev->name      = "NET";
    dev->baseAddress = baseAddress;
    dev->size      = NET_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = netReadByte;
    dev->readWord  = netReadWord;
    dev->writeByte = netWriteByte;
    dev->writeWord = netWriteWord;
    dev->reset     = netReset;
    dev->destroy   = netDestroy;
    dev->getInterruptLevel = netGetIRQ;
    dev->tick      = NULL;
    return dev;
}

void networkSetBus(Device *dev, struct Bus *bus) {
    NetworkState *s = (NetworkState *)dev->data;
    s->bus = bus;
}

void networkInjectFrame(Device *dev, const u8 *frame, u16 length) {
    NetworkState *s = (NetworkState *)dev->data;
    if (length > NET_MAX_FRAME) return;
    memcpy(s->rxBuf, frame, length);
    s->rxLen = length;
    /* DMA received frame into bus memory */
    if (s->bus) {
        for (u16 i = 0; i < length; i++)
            busWriteByte(s->bus, s->dmaAddress + i, s->rxBuf[i]);
    }
    s->status |= 1;  /* rx ready */
}

u16 networkPullFrame(Device *dev, u8 *outFrame, u16 maxLen) {
    NetworkState *s = (NetworkState *)dev->data;
    if (!(s->status & 2)) return 0;
    u16 len = s->txLen;
    if (len > maxLen) len = maxLen;
    memcpy(outFrame, s->txBuf, len);
    s->status &= ~2;  /* clear tx done */
    return len;
}
