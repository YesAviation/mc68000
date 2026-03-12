/* ===========================================================================
 *  mkdisk.c — Create a MiniMint disk image
 *
 *  Usage: mkdisk <output.img> <kernel.bin> [size_mb]
 *
 *  Creates a raw disk image with:
 *    Sector 0:  Boot record (magic, kernel size, kernel sector start)
 *    Sector 1+: Kernel binary
 *    Remaining: Zeroed (free space for filesystem later)
 * =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR_SIZE   512
#define MAGIC_WORD    0x4D4D   /* "MM" = MiniMint */

/* Boot record layout (sector 0):
 *   Offset 0:  u16 magic ($4D4D)
 *   Offset 2:  u16 version (1)
 *   Offset 4:  u32 kernel_size (bytes)
 *   Offset 8:  u32 kernel_start_sector
 *   Offset 12: u32 total_sectors
 *   Offset 16: 496 bytes reserved (zero)
 */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output.img> <kernel.bin> [size_mb]\n", argv[0]);
        return 1;
    }

    const char *outPath    = argv[1];
    const char *kernelPath = argv[2];
    int sizeMB = (argc > 3) ? atoi(argv[3]) : 4;  /* default 4 MB disk */

    /* Read kernel binary */
    FILE *kf = fopen(kernelPath, "rb");
    if (!kf) {
        fprintf(stderr, "Error: Cannot open kernel: %s\n", kernelPath);
        return 1;
    }
    fseek(kf, 0, SEEK_END);
    long kernelSize = ftell(kf);
    fseek(kf, 0, SEEK_SET);

    uint8_t *kernel = malloc(kernelSize);
    fread(kernel, 1, kernelSize, kf);
    fclose(kf);

    printf("Kernel: %s (%ld bytes)\n", kernelPath, kernelSize);

    /* Calculate geometry */
    uint32_t totalSectors = (sizeMB * 1024 * 1024) / SECTOR_SIZE;
    uint32_t kernelSectors = (kernelSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint32_t kernelStart = 1;  /* sector 1 */

    printf("Disk:   %s (%d MB, %u sectors)\n", outPath, sizeMB, totalSectors);
    printf("Kernel: %u sectors starting at sector %u\n", kernelSectors, kernelStart);

    /* Create output file */
    FILE *of = fopen(outPath, "wb");
    if (!of) {
        fprintf(stderr, "Error: Cannot create output: %s\n", outPath);
        free(kernel);
        return 1;
    }

    /* Write boot record (sector 0) */
    uint8_t bootRecord[SECTOR_SIZE];
    memset(bootRecord, 0, SECTOR_SIZE);

    /* Big-endian writes (MC68000 is big-endian) */
    bootRecord[0] = (MAGIC_WORD >> 8) & 0xFF;
    bootRecord[1] = MAGIC_WORD & 0xFF;
    bootRecord[2] = 0x00;  /* version high */
    bootRecord[3] = 0x01;  /* version low = 1 */
    bootRecord[4] = (kernelSize >> 24) & 0xFF;
    bootRecord[5] = (kernelSize >> 16) & 0xFF;
    bootRecord[6] = (kernelSize >> 8) & 0xFF;
    bootRecord[7] = kernelSize & 0xFF;
    bootRecord[8]  = (kernelStart >> 24) & 0xFF;
    bootRecord[9]  = (kernelStart >> 16) & 0xFF;
    bootRecord[10] = (kernelStart >> 8) & 0xFF;
    bootRecord[11] = kernelStart & 0xFF;
    bootRecord[12] = (totalSectors >> 24) & 0xFF;
    bootRecord[13] = (totalSectors >> 16) & 0xFF;
    bootRecord[14] = (totalSectors >> 8) & 0xFF;
    bootRecord[15] = totalSectors & 0xFF;

    fwrite(bootRecord, 1, SECTOR_SIZE, of);

    /* Write kernel (sectors 1 through 1+N) */
    fwrite(kernel, 1, kernelSize, of);

    /* Pad remainder of last kernel sector */
    long kernelPad = (kernelSectors * SECTOR_SIZE) - kernelSize;
    if (kernelPad > 0) {
        uint8_t *pad = calloc(1, kernelPad);
        fwrite(pad, 1, kernelPad, of);
        free(pad);
    }

    /* Fill rest of disk with zeros */
    long remaining = ((long)totalSectors - 1 - kernelSectors) * SECTOR_SIZE;
    if (remaining > 0) {
        /* Write in chunks to avoid huge malloc */
        uint8_t zeros[SECTOR_SIZE];
        memset(zeros, 0, SECTOR_SIZE);
        long left = remaining;
        while (left > 0) {
            long chunk = (left > SECTOR_SIZE) ? SECTOR_SIZE : left;
            fwrite(zeros, 1, chunk, of);
            left -= chunk;
        }
    }

    fclose(of);
    free(kernel);

    printf("Disk image created: %s\n", outPath);
    return 0;
}
