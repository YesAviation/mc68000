/* ===========================================================================
 *  bus.h — Memory bus with address decoding and device mapping
 *
 *  The MC68000 has a 24-bit address bus (16 MB address space).
 *  All word/long accesses must be word-aligned (odd addresses cause
 *  an address error exception).
 *
 *  Memory map is configurable. Typical layout:
 *    $000000-$0003FF  Exception vector table (1 KB, in RAM or ROM)
 *    $000000-$0FFFFF  RAM (1 MB default)
 *    $F00000-$F0FFFF  ROM (64 KB default)
 *    $E00000-$E0FFFF  I/O devices (mapped by base address)
 * =========================================================================== */
#ifndef M68K_BUS_H
#define M68K_BUS_H

#include "common/types.h"

/* Forward declaration for device interface */
typedef struct Device Device;

/* Forward declaration for CPU (needed for address error exceptions) */
typedef struct Cpu Cpu;

/* Maximum number of devices on the bus */
#define BUS_MAX_DEVICES 16

/* ── Bus structure ── */
typedef struct Bus {
    /* CPU back-pointer (for address error exceptions on odd word/long access) */
    Cpu   *cpu;

    /* RAM */
    u8    *ram;
    u32    ramBase;
    u32    ramSize;

    /* ROM */
    u8    *rom;
    u32    romBase;
    u32    romSize;

    /* ROM overlay: when true, reads from $000000-$00FFFF return ROM content.
     * Set to true on reset (like real 68K hardware), cleared by writing
     * to the overlay latch register at MEM_ROM_OVERLAY_LATCH. */
    bool   romOverlay;

    /* Memory-mapped devices */
    Device *devices[BUS_MAX_DEVICES];
    int     deviceCount;
} Bus;

/* ── Lifecycle ── */
Bus *busCreate(u32 ramSize, u32 romSize, u32 romBase);
void busInit(Bus *bus, u32 ramSize, u32 romSize, u32 romBase);
void busDestroy(Bus *bus);
void busReset(Bus *bus);

/* ── Load ROM image into ROM area ── */
bool busLoadRom(Bus *bus, const u8 *data, u32 size);

/* ── Device mapping ── */
bool busAttachDevice(Bus *bus, Device *device);

/* ── Read operations ── */
u8   busReadByte(Bus *bus, u32 address);
u16  busReadWord(Bus *bus, u32 address);
u32  busReadLong(Bus *bus, u32 address);

/* ── Write operations ── */
void busWriteByte(Bus *bus, u32 address, u8 value);
void busWriteWord(Bus *bus, u32 address, u16 value);
void busWriteLong(Bus *bus, u32 address, u32 value);

/* ── Interrupt acknowledge ──
 * Perform IACK cycle: find the device asserting the given interrupt level
 * and ask it for a vector number.
 * Returns: -1 = autovector (VPA), 0 = spurious, 1-255 = vector number */
int busAcknowledgeInterrupt(Bus *bus, int level);

#endif /* M68K_BUS_H */
