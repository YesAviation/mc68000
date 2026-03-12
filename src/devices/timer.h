/* timer.h — Programmable timer/counter device */
#ifndef M68K_TIMER_H
#define M68K_TIMER_H
#include "devices/device.h"

typedef struct { u32 counter; u32 reload; u8 control; int interruptLevel; bool fired; } TimerState;

Device *timerCreate(u32 baseAddress, int interruptLevel);
#endif
