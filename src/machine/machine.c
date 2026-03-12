/* machine.c — Full system integration */
#include "machine/machine.h"
#include "devices/uart.h"
#include "devices/timer.h"
#include "devices/pic.h"
#include "devices/parallel.h"
#include "devices/rtc.h"
#include "devices/audio.h"
#include "devices/network.h"
#include "devices/storage.h"
#include "devices/video.h"
#include "common/log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── lifecycle ───────────────────────────────────────── */

Machine *machineCreate(void) {
    Machine *m = calloc(1, sizeof(Machine));
    if (!m) return NULL;

    /* Bus — RAM + ROM */
    busInit(&m->bus, MACHINE_RAM_SIZE, MACHINE_ROM_SIZE, MACHINE_ROM_BASE);

    /* Devices */
    m->uart     = uartCreate(MACHINE_UART_BASE, 4);
    m->timer    = timerCreate(MACHINE_TIMER_BASE, 5);
    m->pic      = picCreate(MACHINE_PIC_BASE);
    m->parallel = parallelCreate(MACHINE_PARALLEL_BASE, 3);
    m->rtc      = rtcCreate(MACHINE_RTC_BASE);
    m->audio    = audioCreate(MACHINE_AUDIO_BASE, 4);
    m->network  = networkCreate(MACHINE_NET_BASE, 2);
    m->storage  = storageCreate(MACHINE_STORAGE_BASE, 6);
    m->video    = videoCreate(MACHINE_VIDEO_BASE, 1,
                              MACHINE_CPU_CLOCK_HZ, MACHINE_REFRESH_HZ);

    busAttachDevice(&m->bus, m->uart);
    busAttachDevice(&m->bus, m->timer);
    busAttachDevice(&m->bus, m->pic);
    busAttachDevice(&m->bus, m->parallel);
    busAttachDevice(&m->bus, m->rtc);
    busAttachDevice(&m->bus, m->audio);
    busAttachDevice(&m->bus, m->network);
    busAttachDevice(&m->bus, m->storage);
    busAttachDevice(&m->bus, m->video);

    /* Give DMA-capable devices a bus handle */
    storageSetBus(m->storage, &m->bus);
    networkSetBus(m->network, &m->bus);

    /* CPU */
    cpuInit(&m->cpu, &m->bus);

    m->targetCyclesPerFrame = MACHINE_CPU_CLOCK_HZ / MACHINE_REFRESH_HZ;
    m->running = false;

    logMessage(LOG_INFO, MOD_MACHINE, "Machine created: %u KB RAM, %u KB ROM",
               MACHINE_RAM_SIZE / 1024, MACHINE_ROM_SIZE / 1024);
    return m;
}

void machineDestroy(Machine *m) {
    if (!m) return;
    /* Devices are destroyed via bus */
    busDestroy(&m->bus);
    cpuDestroy(&m->cpu);
    free(m);
}

void machineReset(Machine *m) {
    busReset(&m->bus);
    cpuReset(&m->cpu);
    logMessage(LOG_INFO, MOD_MACHINE, "Machine reset");
}

/* ── ROM / Disk loading ──────────────────────────────── */

bool machineLoadRom(Machine *m, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        logMessage(LOG_ERROR, MOD_MACHINE, "Cannot open ROM: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || (u32)size > MACHINE_ROM_SIZE) {
        logMessage(LOG_ERROR, MOD_MACHINE, "ROM size %ld exceeds %u bytes",
                   size, MACHINE_ROM_SIZE);
        fclose(f);
        return false;
    }

    u8 *buf = malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        logMessage(LOG_ERROR, MOD_MACHINE, "Failed to read ROM");
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);

    busLoadRom(&m->bus, buf, (u32)size);
    free(buf);

    logMessage(LOG_INFO, MOD_MACHINE, "Loaded ROM: %s (%ld bytes)", path, size);
    return true;
}

bool machineAttachDisk(Machine *m, const char *path, bool readOnly) {
    FILE *f = fopen(path, readOnly ? "rb" : "r+b");
    if (!f) {
        logMessage(LOG_ERROR, MOD_MACHINE, "Cannot open disk image: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8 *data = malloc((size_t)size);
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return false;
    }
    fclose(f);

    storageAttachImage(m->storage, data, (u32)size, readOnly);
    logMessage(LOG_INFO, MOD_MACHINE, "Attached disk: %s (%ld bytes, %s)",
               path, size, readOnly ? "RO" : "RW");
    return true;
    /* NOTE: data leak — in a real implementation we'd track this for cleanup */
}

/* ── execution ───────────────────────────────────────── */

u32 machineStep(Machine *m) {
    u32 cycles = cpuStep(&m->cpu);
    machineTickDevices(m, cycles);
    machinePollInterrupts(m);
    return cycles;
}

void machineRunFrame(Machine *m) {
    u64 cyclesBudget = m->targetCyclesPerFrame;
    u64 start = m->cpu.totalCycles;
    while (m->cpu.totalCycles - start < cyclesBudget && !m->cpu.halted) {
        machineStep(m);
    }
}

void machineRun(Machine *m) {
    m->running = true;
    while (m->running && !m->cpu.halted) {
        machineStep(m);
    }
}

void machineTickDevices(Machine *m, u32 cycles) {
    Bus *b = &m->bus;
    for (int i = 0; i < b->deviceCount; i++) {
        if (b->devices[i] && b->devices[i]->tick)
            b->devices[i]->tick(b->devices[i], cycles);
    }
}

void machinePollInterrupts(Machine *m) {
    int highest = 0;
    Bus *b = &m->bus;
    for (int i = 0; i < b->deviceCount; i++) {
        if (b->devices[i] && b->devices[i]->getInterruptLevel) {
            int level = b->devices[i]->getInterruptLevel(b->devices[i]);
            if (level > highest) highest = level;
        }
    }
    if (highest > 0) {
        cpuRaiseInterrupt(&m->cpu, highest);
    }
}
