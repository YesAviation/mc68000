/* parallel.c — Parallel I/O port */
#include "devices/parallel.h"
#include <stdlib.h>

static u8 parReadByte(Device *dev, u32 off) {
    ParallelState *s = (ParallelState *)dev->data;
    switch (off) {
        case 0: /* PORT_A read: output bits from data, input bits from external */
            return (s->portAData & s->portADir) | (s->portAInput & ~s->portADir);
        case 1: return s->portADir;
        case 2:
            return (s->portBData & s->portBDir) | (s->portBInput & ~s->portBDir);
        case 3: return s->portBDir;
        case 4: return s->ctrl;
        case 5: return s->status;
        default: return 0;
    }
}

static u16 parReadWord(Device *dev, u32 off) {
    return ((u16)parReadByte(dev, off) << 8) | parReadByte(dev, off + 1);
}

static void parWriteByte(Device *dev, u32 off, u8 val) {
    ParallelState *s = (ParallelState *)dev->data;
    switch (off) {
        case 0: s->portAData = val; break;
        case 1: s->portADir  = val; break;
        case 2: s->portBData = val; break;
        case 3: s->portBDir  = val; break;
        case 4: s->ctrl      = val; break;
        case 5: s->status   &= ~val; break;  /* write-1-clear */
        default: break;
    }
}

static void parWriteWord(Device *dev, u32 off, u16 val) {
    parWriteByte(dev, off, (u8)(val >> 8));
    parWriteByte(dev, off + 1, (u8)val);
}

static int parGetIRQ(Device *dev) {
    ParallelState *s = (ParallelState *)dev->data;
    if ((s->ctrl & 2) && s->status) return s->interruptLevel;
    return 0;
}

static void parReset(Device *dev) {
    ParallelState *s = (ParallelState *)dev->data;
    s->portAData = 0; s->portADir = 0; s->portAInput = 0;
    s->portBData = 0; s->portBDir = 0; s->portBInput = 0;
    s->ctrl = 0; s->status = 0;
}

static void parDestroy(Device *dev) { free(dev->data); free(dev); }

Device *parallelCreate(u32 baseAddress, int interruptLevel) {
    Device *dev     = calloc(1, sizeof(Device));
    ParallelState *s = calloc(1, sizeof(ParallelState));
    s->interruptLevel = interruptLevel;

    dev->name      = "PARALLEL";
    dev->baseAddress = baseAddress;
    dev->size      = PARALLEL_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = parReadByte;
    dev->readWord  = parReadWord;
    dev->writeByte = parWriteByte;
    dev->writeWord = parWriteWord;
    dev->reset     = parReset;
    dev->destroy   = parDestroy;
    dev->getInterruptLevel = parGetIRQ;
    dev->tick      = NULL;
    return dev;
}

void parallelSetInputA(Device *dev, u8 value) {
    ParallelState *s = (ParallelState *)dev->data;
    s->portAInput = value;
    s->status |= 1;
}

void parallelSetInputB(Device *dev, u8 value) {
    ParallelState *s = (ParallelState *)dev->data;
    s->portBInput = value;
    s->status |= 2;
}

u8 parallelGetOutputA(Device *dev) {
    ParallelState *s = (ParallelState *)dev->data;
    return s->portAData & s->portADir;
}

u8 parallelGetOutputB(Device *dev) {
    ParallelState *s = (ParallelState *)dev->data;
    return s->portBData & s->portBDir;
}
