/* parallel.h — Parallel I/O port (68230-inspired) */
#ifndef M68K_PARALLEL_H
#define M68K_PARALLEL_H

#include "devices/device.h"

/*
 * Dual 8-bit parallel port with direction control.
 *
 * Register map (byte):
 *   +0  PORT_A_DATA  (R/W)
 *   +1  PORT_A_DIR   (W)  — 1 = output, 0 = input
 *   +2  PORT_B_DATA  (R/W)
 *   +3  PORT_B_DIR   (W)
 *   +4  CTRL         (W)  — bit0 = handshake enable, bit1 = IRQ on input
 *   +5  STATUS       (R)  — bit0 = port A input ready, bit1 = port B input ready
 */

#define PARALLEL_DEVICE_SIZE 8

typedef struct {
    u8  portAData;
    u8  portADir;          /* direction: 1=output, 0=input */
    u8  portAInput;        /* external input latch         */
    u8  portBData;
    u8  portBDir;
    u8  portBInput;
    u8  ctrl;
    u8  status;
    int interruptLevel;
} ParallelState;

Device *parallelCreate(u32 baseAddress, int interruptLevel);

/* Host side: set external input lines */
void parallelSetInputA(Device *dev, u8 value);
void parallelSetInputB(Device *dev, u8 value);

/* Host side: read output lines */
u8 parallelGetOutputA(Device *dev);
u8 parallelGetOutputB(Device *dev);

#endif /* M68K_PARALLEL_H */
