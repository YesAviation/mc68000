/* rtc.c — Real-time clock (backed by host system time) */
#include "devices/rtc.h"
#include <stdlib.h>
#include <time.h>

static void rtcLatch(RtcState *s) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;
    s->seconds = (u8)t->tm_sec;
    s->minutes = (u8)t->tm_min;
    s->hours   = (u8)t->tm_hour;
    s->day     = (u8)t->tm_mday;
    s->month   = (u8)(t->tm_mon + 1);
    s->year    = (u16)(t->tm_year + 1900);
}

static u8 rtcReadByte(Device *dev, u32 off) {
    RtcState *s = (RtcState *)dev->data;
    if (!s->latched) rtcLatch(s);
    switch (off) {
        case 0: return s->seconds;
        case 1: return s->minutes;
        case 2: return s->hours;
        case 3: return s->day;
        case 4: return s->month;
        case 5: return (u8)(s->year & 0xFF);
        case 6: return (u8)(s->year >> 8);
        case 7: return s->ctrl;
        default: return 0;
    }
}

static u16 rtcReadWord(Device *dev, u32 off) {
    return ((u16)rtcReadByte(dev, off) << 8) | rtcReadByte(dev, off + 1);
}

static void rtcWriteByte(Device *dev, u32 off, u8 val) {
    RtcState *s = (RtcState *)dev->data;
    if (off == 7) {
        s->ctrl = val;
        s->latched = (val & 1) != 0;
        if (s->latched) rtcLatch(s);
    }
}

static void rtcWriteWord(Device *dev, u32 off, u16 val) {
    rtcWriteByte(dev, off, (u8)(val >> 8));
}

static void rtcReset(Device *dev) {
    RtcState *s = (RtcState *)dev->data;
    s->ctrl = 0; s->latched = false;
}

static void rtcDestroy(Device *dev) { free(dev->data); free(dev); }

Device *rtcCreate(u32 baseAddress) {
    Device *dev  = calloc(1, sizeof(Device));
    RtcState *s  = calloc(1, sizeof(RtcState));
    dev->name      = "RTC";
    dev->baseAddress = baseAddress;
    dev->size      = RTC_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = rtcReadByte;
    dev->readWord  = rtcReadWord;
    dev->writeByte = rtcWriteByte;
    dev->writeWord = rtcWriteWord;
    dev->reset     = rtcReset;
    dev->destroy   = rtcDestroy;
    dev->getInterruptLevel = NULL;
    dev->tick      = NULL;
    return dev;
}
