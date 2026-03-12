/* rtc.h — Real-time clock */
#ifndef M68K_RTC_H
#define M68K_RTC_H

#include "devices/device.h"

/*
 * Simple RTC backed by host system time.
 *
 * Register map (byte-accessible):
 *   +0  SECONDS   (R)
 *   +1  MINUTES   (R)
 *   +2  HOURS     (R)
 *   +3  DAY       (R)  1-31
 *   +4  MONTH     (R)  1-12
 *   +5  YEAR_LO   (R)  year mod 256
 *   +6  YEAR_HI   (R)  year / 256
 *   +7  CTRL      (W)  bit0 = latch (freeze read values)
 */

#define RTC_DEVICE_SIZE 8

typedef struct {
    u8  seconds;
    u8  minutes;
    u8  hours;
    u8  day;
    u8  month;
    u16 year;
    u8  ctrl;
    bool latched;
} RtcState;

Device *rtcCreate(u32 baseAddress);

#endif /* M68K_RTC_H */
