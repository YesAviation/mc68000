/* pic.h — Programmable Interrupt Controller */
#ifndef M68K_PIC_H
#define M68K_PIC_H

#include "devices/device.h"

#define PIC_MAX_SOURCES 8

typedef struct {
    u8  pendingMask;    /* IRQ lines asserted          */
    u8  enableMask;     /* Which lines are enabled     */
    u8  serviceMask;    /* Lines currently being served*/
    u8  priorityLevel;  /* Base priority (1-7)         */
    int interruptLevel; /* Active output to CPU (0=none) */
} PicState;

Device *picCreate(u32 baseAddress);

/* Called by other devices to raise / lower an IRQ line (0-7) */
void picAssertLine(Device *dev, int line);
void picDeassertLine(Device *dev, int line);

#endif /* M68K_PIC_H */
