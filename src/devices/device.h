/* ===========================================================================
 *  device.h — Base device interface (virtual method table pattern in C)
 *
 *  All peripherals implement this interface. The bus uses the function
 *  pointers for memory-mapped I/O dispatch.
 * =========================================================================== */
#ifndef M68K_DEVICE_H
#define M68K_DEVICE_H

#include "common/types.h"

typedef struct Device {
    const char *name;
    u32         baseAddress;     /* Base address in 68000 address space */
    u32         size;            /* Size of address range */

    /* Memory-mapped I/O callbacks */
    u8   (*readByte)(struct Device *dev, u32 offset);
    u16  (*readWord)(struct Device *dev, u32 offset);
    void (*writeByte)(struct Device *dev, u32 offset, u8 value);
    void (*writeWord)(struct Device *dev, u32 offset, u16 value);

    /* Lifecycle */
    void (*reset)(struct Device *dev);
    void (*destroy)(struct Device *dev);

    /* Interrupt support */
    int  (*getInterruptLevel)(struct Device *dev);  /* 0=none, 1-7=level */

    /* Interrupt acknowledge: called during IACK cycle when CPU acknowledges
     * this device's interrupt at the given level.
     * Return values:
     *   -1  = autovector (device asserts VPA; CPU uses vector 24+level)
     *    0  = spurious (no response; CPU uses vector 24)
     *  1-255 = vectored interrupt (device provides vector number on data bus)
     * If NULL, defaults to autovector behavior. */
    int  (*acknowledgeInterrupt)(struct Device *dev, int level);

    /* Tick: called at some rate for devices that need periodic updates */
    void (*tick)(struct Device *dev, u32 cycles);

    /* Private data (device-specific state) */
    void *data;
} Device;

#endif /* M68K_DEVICE_H */
