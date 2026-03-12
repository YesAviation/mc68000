/* pic.c — Programmable Interrupt Controller */
#include "devices/pic.h"
#include <stdlib.h>
#include <string.h>

/*
 * Simple priority-encoded interrupt controller.
 * 8 input lines map to a single output interrupt level (1-7).
 * Registers (byte-accessible):
 *   +0  pendingMask  (R)
 *   +1  enableMask   (R/W)
 *   +2  serviceMask  (R)  — acknowledge by writing bit
 *   +3  priorityLevel(R/W)
 */

static void picRecalc(PicState *s) {
    u8 active = s->pendingMask & s->enableMask & ~s->serviceMask;
    s->interruptLevel = active ? s->priorityLevel : 0;
}

static u8 picReadByte(Device *dev, u32 offset) {
    PicState *s = (PicState *)dev->data;
    switch (offset) {
        case 0: return s->pendingMask;
        case 1: return s->enableMask;
        case 2: return s->serviceMask;
        case 3: return s->priorityLevel;
        default: return 0;
    }
}

static u16 picReadWord(Device *dev, u32 offset) {
    return ((u16)picReadByte(dev, offset) << 8) | picReadByte(dev, offset + 1);
}

static void picWriteByte(Device *dev, u32 offset, u8 value) {
    PicState *s = (PicState *)dev->data;
    switch (offset) {
        case 1: s->enableMask = value; break;
        case 2: s->serviceMask &= ~value; break;          /* write 1 to ack */
        case 3: s->priorityLevel = value & 7; break;
        default: break;
    }
    picRecalc(s);
}

static void picWriteWord(Device *dev, u32 offset, u16 value) {
    picWriteByte(dev, offset, (u8)(value >> 8));
    picWriteByte(dev, offset + 1, (u8)value);
}

static int picGetIRQ(Device *dev) {
    PicState *s = (PicState *)dev->data;
    return s->interruptLevel;
}

static void picReset(Device *dev) {
    PicState *s = (PicState *)dev->data;
    s->pendingMask = 0;
    s->enableMask = 0;
    s->serviceMask = 0;
    s->priorityLevel = 0;
    s->interruptLevel = 0;
}

static void picDestroy(Device *dev) { free(dev->data); free(dev); }

Device *picCreate(u32 baseAddress) {
    Device *dev = calloc(1, sizeof(Device));
    PicState *s = calloc(1, sizeof(PicState));
    dev->name = "PIC";
    dev->baseAddress = baseAddress;
    dev->size = 8;
    dev->data = s;
    dev->readByte = picReadByte;
    dev->readWord = picReadWord;
    dev->writeByte = picWriteByte;
    dev->writeWord = picWriteWord;
    dev->reset = picReset;
    dev->destroy = picDestroy;
    dev->getInterruptLevel = picGetIRQ;
    dev->tick = NULL;
    return dev;
}

void picAssertLine(Device *dev, int line) {
    PicState *s = (PicState *)dev->data;
    if (line >= 0 && line < PIC_MAX_SOURCES) {
        s->pendingMask |= (1 << line);
        picRecalc(s);
    }
}

void picDeassertLine(Device *dev, int line) {
    PicState *s = (PicState *)dev->data;
    if (line >= 0 && line < PIC_MAX_SOURCES) {
        s->pendingMask &= ~(1 << line);
        picRecalc(s);
    }
}
