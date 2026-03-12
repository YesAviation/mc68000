/* machine.h — Full system: CPU + bus + devices */
#ifndef M68K_MACHINE_H
#define M68K_MACHINE_H

#include "common/types.h"
#include "cpu/cpu.h"
#include "bus/bus.h"
#include "devices/device.h"

/*
 * Default memory map
 *   $000000 – $0FFFFF   RAM   (1 MB)
 *   $E00000 – $E0FFFF   I/O   (64 KB window)
 *   $F00000 – $F0FFFF   ROM   (64 KB)
 *
 * I/O sub-map (offsets from $E00000):
 *   $0000  UART
 *   $0100  TIMER
 *   $0200  PIC
 *   $0300  PARALLEL
 *   $0400  RTC
 *   $0500  AUDIO
 *   $0600  NETWORK
 *   $1000  STORAGE
 *   $8000  VIDEO (needs large range for VRAM)
 */

#define MACHINE_RAM_SIZE     0x100000   /* 1 MB  */
#define MACHINE_ROM_BASE     0xF00000
#define MACHINE_ROM_SIZE     0x010000   /* 64 KB */
#define MACHINE_IO_BASE      0xE00000

#define MACHINE_UART_BASE    (MACHINE_IO_BASE + 0x0000)
#define MACHINE_TIMER_BASE   (MACHINE_IO_BASE + 0x0100)
#define MACHINE_PIC_BASE     (MACHINE_IO_BASE + 0x0200)
#define MACHINE_PARALLEL_BASE (MACHINE_IO_BASE + 0x0300)
#define MACHINE_RTC_BASE     (MACHINE_IO_BASE + 0x0400)
#define MACHINE_AUDIO_BASE   (MACHINE_IO_BASE + 0x0500)
#define MACHINE_NET_BASE     (MACHINE_IO_BASE + 0x0600)
#define MACHINE_STORAGE_BASE (MACHINE_IO_BASE + 0x1000)
#define MACHINE_VIDEO_BASE   (MACHINE_IO_BASE + 0x8000)

#define MACHINE_CPU_CLOCK_HZ 8000000   /* 8 MHz */
#define MACHINE_REFRESH_HZ   60

typedef struct Machine {
    Cpu     cpu;
    Bus     bus;

    /* Named device handles for easy host-side access */
    Device *uart;
    Device *timer;
    Device *pic;
    Device *parallel;
    Device *rtc;
    Device *audio;
    Device *network;
    Device *storage;
    Device *video;

    /* Execution state */
    bool    running;
    u64     targetCyclesPerFrame;
} Machine;

/* Lifecycle */
Machine *machineCreate(void);
void     machineDestroy(Machine *m);
void     machineReset(Machine *m);

/* Load ROM image (binary, loaded at MACHINE_ROM_BASE) */
bool machineLoadRom(Machine *m, const char *path);

/* Attach a flat disk image file for the storage controller */
bool machineAttachDisk(Machine *m, const char *path, bool readOnly);

/* Run one instruction; returns cycles consumed */
u32 machineStep(Machine *m);

/* Run for one video frame (~CPU_CLOCK / REFRESH_HZ cycles) */
void machineRunFrame(Machine *m);

/* Run until stopped */
void machineRun(Machine *m);

/* Tick all devices for the given number of CPU cycles */
void machineTickDevices(Machine *m, u32 cycles);

/* Check device interrupts and present highest to CPU */
void machinePollInterrupts(Machine *m);

#endif /* M68K_MACHINE_H */
