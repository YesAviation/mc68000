/* storage.c — Block storage device (disk image) */
#include "devices/storage.h"
#include "bus/bus.h"
#include <stdlib.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────── */

static void storageExecRead(StorageState *s) {
    if (!s->image || !s->bus) { s->status = STORAGE_STATUS_ERROR; return; }
    u32 startByte = s->sector * STORAGE_SECTOR_SIZE;
    u32 totalBytes = (u32)s->sectorCount * STORAGE_SECTOR_SIZE;
    if (startByte + totalBytes > s->imageSize) { s->status = STORAGE_STATUS_ERROR; return; }
    u32 addr = s->dmaAddress;
    for (u32 i = 0; i < totalBytes; i++) {
        busWriteByte(s->bus, addr + i, s->image[startByte + i]);
    }
    s->status = STORAGE_STATUS_READY;
}

static void storageExecWrite(StorageState *s) {
    if (!s->image || !s->bus || s->readOnly) { s->status = STORAGE_STATUS_ERROR; return; }
    u32 startByte = s->sector * STORAGE_SECTOR_SIZE;
    u32 totalBytes = (u32)s->sectorCount * STORAGE_SECTOR_SIZE;
    if (startByte + totalBytes > s->imageSize) { s->status = STORAGE_STATUS_ERROR; return; }
    u32 addr = s->dmaAddress;
    for (u32 i = 0; i < totalBytes; i++) {
        s->image[startByte + i] = busReadByte(s->bus, addr + i);
    }
    s->status = STORAGE_STATUS_READY;
}

/* ── register access ─────────────────────────────────── */

static u8 storageReadByte(Device *dev, u32 off) {
    StorageState *s = (StorageState *)dev->data;
    if (off == STORAGE_REG_STATUS)     return (u8)(s->status >> 8);
    if (off == STORAGE_REG_STATUS + 1) return (u8)s->status;
    return 0;
}

static u16 storageReadWord(Device *dev, u32 off) {
    StorageState *s = (StorageState *)dev->data;
    if (off == STORAGE_REG_STATUS) return s->status;
    return 0;
}

static void storageWriteByte(Device *dev, u32 off, u8 val) {
    (void)dev; (void)off; (void)val;
    /* byte writes not supported — use word access */
}

static void storageWriteWord(Device *dev, u32 off, u16 val) {
    StorageState *s = (StorageState *)dev->data;
    switch (off) {
        case STORAGE_REG_CMD:
            s->cmd = val;
            if (val == STORAGE_CMD_READ)  storageExecRead(s);
            if (val == STORAGE_CMD_WRITE) storageExecWrite(s);
            break;
        case STORAGE_REG_SECTOR_HI:
            s->sector = ((u32)val << 16) | (s->sector & 0xFFFF);
            break;
        case STORAGE_REG_SECTOR_LO:
            s->sector = (s->sector & 0xFFFF0000) | val;
            break;
        case STORAGE_REG_DMA_HI:
            s->dmaAddress = ((u32)val << 16) | (s->dmaAddress & 0xFFFF);
            break;
        case STORAGE_REG_DMA_LO:
            s->dmaAddress = (s->dmaAddress & 0xFFFF0000) | val;
            break;
        case STORAGE_REG_COUNT:
            s->sectorCount = val;
            break;
        default: break;
    }
}

static int storageGetIRQ(Device *dev) {
    StorageState *s = (StorageState *)dev->data;
    /* fire interrupt on completion (non-busy + ready) */
    return (s->status & STORAGE_STATUS_READY) ? s->interruptLevel : 0;
}

static void storageReset(Device *dev) {
    StorageState *s = (StorageState *)dev->data;
    s->cmd = 0; s->status = STORAGE_STATUS_READY;
    s->sector = 0; s->dmaAddress = 0; s->sectorCount = 0;
}

static void storageDestroy(Device *dev) {
    /* NOTE: we do NOT free image — caller owns it */
    free(dev->data);
    free(dev);
}

/* ── constructor ─────────────────────────────────────── */

Device *storageCreate(u32 baseAddress, int interruptLevel) {
    Device *dev    = calloc(1, sizeof(Device));
    StorageState *s = calloc(1, sizeof(StorageState));
    s->interruptLevel = interruptLevel;
    s->status         = STORAGE_STATUS_READY;

    dev->name      = "STORAGE";
    dev->baseAddress = baseAddress;
    dev->size      = STORAGE_DEVICE_SIZE;
    dev->data      = s;
    dev->readByte  = storageReadByte;
    dev->readWord  = storageReadWord;
    dev->writeByte = storageWriteByte;
    dev->writeWord = storageWriteWord;
    dev->reset     = storageReset;
    dev->destroy   = storageDestroy;
    dev->getInterruptLevel = storageGetIRQ;
    dev->tick      = NULL;
    return dev;
}

void storageAttachImage(Device *dev, u8 *data, u32 size, bool readOnly) {
    StorageState *s = (StorageState *)dev->data;
    s->image = data;
    s->imageSize = size;
    s->totalSectors = size / STORAGE_SECTOR_SIZE;
    s->readOnly = readOnly;
}

void storageDetachImage(Device *dev) {
    StorageState *s = (StorageState *)dev->data;
    s->image = NULL;
    s->imageSize = 0;
    s->totalSectors = 0;
}

void storageSetBus(Device *dev, struct Bus *bus) {
    StorageState *s = (StorageState *)dev->data;
    s->bus = bus;
}
