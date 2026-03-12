/* timer.c — Programmable timer/counter */
#include "devices/timer.h"
#include <stdlib.h>
#include <string.h>

static u8 timerReadByte(Device *dev, u32 offset) {
    TimerState *s = (TimerState *)dev->data;
    switch (offset) {
        case 0: return (u8)(s->counter >> 8);
        case 1: return (u8)(s->counter & 0xFF);
        case 2: return s->control;
        default: return 0;
    }
}
static u16 timerReadWord(Device *dev, u32 offset) {
    TimerState *s = (TimerState *)dev->data;
    if (offset == 0) return (u16)s->counter;
    return (u16)timerReadByte(dev, offset);
}
static void timerWriteByte(Device *dev, u32 offset, u8 value) {
    TimerState *s = (TimerState *)dev->data;
    switch (offset) {
        case 0: s->reload = (s->reload & 0x00FF) | ((u32)value << 8); break;
        case 1: s->reload = (s->reload & 0xFF00) | value; break;
        case 2: s->control = value; if (value & 1) { s->counter = s->reload; s->fired = false; } break;
    }
}
static void timerWriteWord(Device *dev, u32 offset, u16 value) {
    if (offset == 0) { TimerState *s = (TimerState *)dev->data; s->reload = value; }
    else timerWriteByte(dev, offset, (u8)value);
}
static void timerTick(Device *dev, u32 cycles) {
    TimerState *s = (TimerState *)dev->data;
    if (!(s->control & 1)) return;
    if (s->counter <= cycles) { s->counter = s->reload; s->fired = true; }
    else s->counter -= cycles;
}
static int timerGetIRQ(Device *dev) { TimerState *s = (TimerState *)dev->data; return s->fired ? s->interruptLevel : 0; }
static void timerReset(Device *dev) { TimerState *s = (TimerState *)dev->data; s->counter = 0; s->reload = 0; s->control = 0; s->fired = false; }
static void timerDestroy(Device *dev) { free(dev->data); free(dev); }

Device *timerCreate(u32 baseAddress, int interruptLevel) {
    Device *dev = calloc(1, sizeof(Device));
    TimerState *s = calloc(1, sizeof(TimerState));
    s->interruptLevel = interruptLevel;
    dev->name = "TIMER"; dev->baseAddress = baseAddress; dev->size = 8;
    dev->data = s; dev->readByte = timerReadByte; dev->readWord = timerReadWord;
    dev->writeByte = timerWriteByte; dev->writeWord = timerWriteWord;
    dev->reset = timerReset; dev->destroy = timerDestroy;
    dev->getInterruptLevel = timerGetIRQ; dev->tick = timerTick;
    return dev;
}
