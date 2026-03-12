/* storage.h — Block storage device (disk image) */
#ifndef M68K_STORAGE_H
#define M68K_STORAGE_H

#include "devices/device.h"

/*
 * Minimal block-oriented storage controller.
 *
 * Register map (word-accessible):
 *   +0x00  CMD       (W)  — 1=read sector, 2=write sector, 3=status
 *   +0x02  STATUS    (R)  — bit0=ready, bit1=error, bit2=busy
 *   +0x04  SECTOR_HI (W)  — sector number high word
 *   +0x06  SECTOR_LO (W)  — sector number low word
 *   +0x08  DMA_HI    (W)  — DMA target address high word
 *   +0x0A  DMA_LO    (W)  — DMA target address low word
 *   +0x0C  COUNT     (W)  — sector count (1–256)
 *
 * Sector size: 512 bytes.
 * The device performs DMA directly to/from the bus.
 */

#define STORAGE_SECTOR_SIZE 512
#define STORAGE_REG_CMD       0x00
#define STORAGE_REG_STATUS    0x02
#define STORAGE_REG_SECTOR_HI 0x04
#define STORAGE_REG_SECTOR_LO 0x06
#define STORAGE_REG_DMA_HI    0x08
#define STORAGE_REG_DMA_LO    0x0A
#define STORAGE_REG_COUNT     0x0C
#define STORAGE_DEVICE_SIZE   0x10

#define STORAGE_CMD_READ   1
#define STORAGE_CMD_WRITE  2
#define STORAGE_CMD_STATUS 3

#define STORAGE_STATUS_READY 0x01
#define STORAGE_STATUS_ERROR 0x02
#define STORAGE_STATUS_BUSY  0x04

typedef struct {
    u16   cmd;
    u16   status;
    u32   sector;
    u32   dmaAddress;
    u16   sectorCount;
    int   interruptLevel;
    /* backing store */
    u8   *image;              /* mmap-like pointer to disk image  */
    u32   imageSize;          /* total bytes                      */
    u32   totalSectors;
    bool  readOnly;
    /* bus reference for DMA */
    struct Bus *bus;
} StorageState;

Device *storageCreate(u32 baseAddress, int interruptLevel);

/* Attach / detach a flat disk image (caller retains ownership of buffer) */
void storageAttachImage(Device *dev, u8 *data, u32 size, bool readOnly);
void storageDetachImage(Device *dev);

/* Set bus reference for DMA transfers */
void storageSetBus(Device *dev, struct Bus *bus);

#endif /* M68K_STORAGE_H */
