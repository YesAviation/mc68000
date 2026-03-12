/* ===========================================================================
 *  bus.c — Memory bus implementation with address decoding
 * =========================================================================== */
#include "bus/bus.h"
#include "bus/memory.h"
#include "devices/device.h"
#include "cpu/exceptions.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ── */

Bus *busCreate(u32 ramSize, u32 romSize, u32 romBase) {
    Bus *bus = calloc(1, sizeof(Bus));
    if (!bus) return NULL;

    /* Allocate RAM */
    bus->ram = calloc(1, ramSize);
    if (!bus->ram) { free(bus); return NULL; }
    bus->ramBase = 0x000000;
    bus->ramSize = ramSize;

    /* Allocate ROM */
    bus->rom = calloc(1, romSize);
    if (!bus->rom) { free(bus->ram); free(bus); return NULL; }
    bus->romBase = romBase;
    bus->romSize = romSize;

    bus->cpu = NULL;
    bus->romOverlay = false;
    bus->deviceCount = 0;

    LOG_INFO(MOD_BUS, "Bus created: RAM=%uKB@$%06X ROM=%uKB@$%06X",
             ramSize / 1024, bus->ramBase,
             romSize / 1024, bus->romBase);

    return bus;
}

void busInit(Bus *bus, u32 ramSize, u32 romSize, u32 romBase) {
    memset(bus, 0, sizeof(Bus));
    bus->ram = calloc(1, ramSize);
    bus->ramBase = 0x000000;
    bus->ramSize = ramSize;
    bus->rom = calloc(1, romSize);
    bus->romBase = romBase;
    bus->romSize = romSize;
    bus->cpu = NULL;
    bus->romOverlay = false;
    bus->deviceCount = 0;
}

void busDestroy(Bus *bus) {
    if (!bus) return;
    free(bus->ram);
    free(bus->rom);
    /* Don't free bus itself — may be embedded or heap-allocated */
}

void busReset(Bus *bus) {
    memset(bus->ram, 0, bus->ramSize);
    bus->romOverlay = true;   /* ROM visible at $000000 until bootloader clears it */
    for (int i = 0; i < bus->deviceCount; i++) {
        if (bus->devices[i] && bus->devices[i]->reset) {
            bus->devices[i]->reset(bus->devices[i]);
        }
    }
}

bool busLoadRom(Bus *bus, const u8 *data, u32 size) {
    if (size > bus->romSize) {
        LOG_ERROR(MOD_BUS, "ROM image (%u bytes) exceeds ROM size (%u bytes)",
                  size, bus->romSize);
        return false;
    }
    memcpy(bus->rom, data, size);
    LOG_INFO(MOD_BUS, "Loaded %u bytes into ROM at $%06X", size, bus->romBase);
    return true;
}

bool busAttachDevice(Bus *bus, Device *device) {
    if (bus->deviceCount >= BUS_MAX_DEVICES) {
        LOG_ERROR(MOD_BUS, "Maximum device count (%d) reached", BUS_MAX_DEVICES);
        return false;
    }
    bus->devices[bus->deviceCount++] = device;
    LOG_INFO(MOD_BUS, "Device '%s' attached at $%06X (size=%u)",
             device->name, device->baseAddress, device->size);
    return true;
}

/* ── Internal: find device at address ── */
static Device *busFindDevice(Bus *bus, u32 address) {
    for (int i = 0; i < bus->deviceCount; i++) {
        Device *dev = bus->devices[i];
        if (address >= dev->baseAddress &&
            address < dev->baseAddress + dev->size) {
            return dev;
        }
    }
    return NULL;
}

/* ── Read operations ── */

u8 busReadByte(Bus *bus, u32 address) {
    address &= ADDR_MASK;

    /* ROM overlay: mirror ROM to $000000 during boot */
    if (bus->romOverlay && address < bus->romSize) {
        return bus->rom[address];
    }

    /* Check ROM first (ROM may overlap RAM at reset vectors) */
    if (address >= bus->romBase && address < bus->romBase + bus->romSize) {
        return bus->rom[address - bus->romBase];
    }

    /* Check RAM */
    if (address >= bus->ramBase && address < bus->ramBase + bus->ramSize) {
        return bus->ram[address - bus->ramBase];
    }

    /* Check devices */
    Device *dev = busFindDevice(bus, address);
    if (dev && dev->readByte) {
        return dev->readByte(dev, address - dev->baseAddress);
    }

    LOG_WARN(MOD_BUS, "Read byte from unmapped address $%06X", address);
    return 0xFF;
}

u16 busReadWord(Bus *bus, u32 address) {
    address &= ADDR_MASK;

    /* MC68000: word access at odd address triggers address error */
    if ((address & 1) && bus->cpu) {
        exceptionAddressError(bus->cpu, address, false, false);
        return 0xFFFF;
    }

    /* ROM overlay: mirror ROM to $000000 during boot */
    if (bus->romOverlay && address + 1 < bus->romSize) {
        u32 off = address;
        return (u16)((bus->rom[off] << 8) | bus->rom[off + 1]);
    }

    /* ROM */
    if (address >= bus->romBase && address + 1 < bus->romBase + bus->romSize) {
        u32 off = address - bus->romBase;
        return (u16)((bus->rom[off] << 8) | bus->rom[off + 1]);
    }

    /* RAM */
    if (address >= bus->ramBase && address + 1 < bus->ramBase + bus->ramSize) {
        u32 off = address - bus->ramBase;
        return (u16)((bus->ram[off] << 8) | bus->ram[off + 1]);
    }

    /* Devices */
    Device *dev = busFindDevice(bus, address);
    if (dev && dev->readWord) {
        return dev->readWord(dev, address - dev->baseAddress);
    }

    /* Fall back to two byte reads */
    return (u16)((busReadByte(bus, address) << 8) |
                  busReadByte(bus, address + 1));
}

u32 busReadLong(Bus *bus, u32 address) {
    return ((u32)busReadWord(bus, address) << 16) |
            (u32)busReadWord(bus, address + 2);
}

/* ── Write operations ── */

void busWriteByte(Bus *bus, u32 address, u8 value) {
    address &= ADDR_MASK;

    /* ROM overlay latch: any write disables the overlay */
    if (address == MEM_ROM_OVERLAY_LATCH) {
        bus->romOverlay = false;
        LOG_INFO(MOD_BUS, "ROM overlay disabled");
        return;
    }

    /* ROM: writes are ignored */
    if (address >= bus->romBase && address < bus->romBase + bus->romSize) {
        LOG_WARN(MOD_BUS, "Write to ROM at $%06X ignored", address);
        return;
    }

    /* RAM */
    if (address >= bus->ramBase && address < bus->ramBase + bus->ramSize) {
        bus->ram[address - bus->ramBase] = value;
        return;
    }

    /* Devices */
    Device *dev = busFindDevice(bus, address);
    if (dev && dev->writeByte) {
        dev->writeByte(dev, address - dev->baseAddress, value);
        return;
    }

    LOG_WARN(MOD_BUS, "Write byte $%02X to unmapped address $%06X", value, address);
}

void busWriteWord(Bus *bus, u32 address, u16 value) {
    address &= ADDR_MASK;

    /* MC68000: word access at odd address triggers address error */
    if ((address & 1) && bus->cpu) {
        exceptionAddressError(bus->cpu, address, true, false);
        return;
    }

    /* ROM overlay latch: any write disables the overlay */
    if (address == MEM_ROM_OVERLAY_LATCH) {
        bus->romOverlay = false;
        LOG_INFO(MOD_BUS, "ROM overlay disabled");
        return;
    }

    /* ROM */
    if (address >= bus->romBase && address < bus->romBase + bus->romSize) {
        return;
    }

    /* RAM */
    if (address >= bus->ramBase && address + 1 < bus->ramBase + bus->ramSize) {
        u32 off = address - bus->ramBase;
        bus->ram[off]     = (u8)(value >> 8);
        bus->ram[off + 1] = (u8)(value & 0xFF);
        return;
    }

    /* Devices */
    Device *dev = busFindDevice(bus, address);
    if (dev && dev->writeWord) {
        dev->writeWord(dev, address - dev->baseAddress, value);
        return;
    }

    /* Fall back */
    busWriteByte(bus, address, (u8)(value >> 8));
    busWriteByte(bus, address + 1, (u8)(value & 0xFF));
}

void busWriteLong(Bus *bus, u32 address, u32 value) {
    busWriteWord(bus, address,     (u16)(value >> 16));
    busWriteWord(bus, address + 2, (u16)(value & 0xFFFF));
}

/* ── Interrupt acknowledge (IACK cycle) ── */
int busAcknowledgeInterrupt(Bus *bus, int level) {
    /* Scan devices for the one asserting this interrupt level */
    for (int i = 0; i < bus->deviceCount; i++) {
        Device *dev = bus->devices[i];
        if (dev && dev->getInterruptLevel && dev->getInterruptLevel(dev) == level) {
            if (dev->acknowledgeInterrupt) {
                int vec = dev->acknowledgeInterrupt(dev, level);
                LOG_DEBUG(MOD_BUS, "IACK level %d: device '%s' returned vector %d",
                          level, dev->name, vec);
                return vec;
            }
            /* Device has no IACK handler — default to autovector */
            LOG_DEBUG(MOD_BUS, "IACK level %d: device '%s' — autovector (no handler)",
                      level, dev->name);
            return -1;
        }
    }

    /* No device claiming this level — spurious interrupt */
    LOG_WARN(MOD_BUS, "IACK level %d: no device responding — spurious", level);
    return 0;
}
