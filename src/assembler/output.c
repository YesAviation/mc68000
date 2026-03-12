/* output.c — Binary output formats */
#include "assembler/output.h"
#include <stdio.h>
#include <string.h>

/* ── raw binary ──────────────────────────────────────── */

bool outputWriteBinary(const Buffer *buf, const char *path) {
    if (!buf || !path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(buf->data, 1, buf->size, f);
    fclose(f);
    return written == buf->size;
}

/* ── Motorola S-record ───────────────────────────────── */

static u8 srecChecksum(const u8 *data, int len) {
    u8 sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return ~sum;
}

bool outputWriteSRecord(const Buffer *buf, u32 baseAddress, const char *path) {
    if (!buf || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    /* S0 header */
    fprintf(f, "S0030000FC\n");

    /* S1 records (16-bit address) or S2 (24-bit) depending on address range */
    u32 offset = 0;
    while (offset < buf->size) {
        u32 addr = baseAddress + offset;
        u32 remaining = buf->size - offset;
        u32 chunkLen = remaining > 16 ? 16 : remaining;

        if (addr <= 0xFFFF) {
            /* S1: 2-byte address */
            u8 rec[64];
            int idx = 0;
            rec[idx++] = (u8)(chunkLen + 3);         /* byte count */
            rec[idx++] = (u8)(addr >> 8);
            rec[idx++] = (u8)(addr);
            for (u32 i = 0; i < chunkLen; i++)
                rec[idx++] = buf->data[offset + i];
            rec[idx] = srecChecksum(rec, idx);
            fprintf(f, "S1");
            for (int i = 0; i <= idx; i++) fprintf(f, "%02X", rec[i]);
            fprintf(f, "\n");
        } else {
            /* S2: 3-byte address */
            u8 rec[64];
            int idx = 0;
            rec[idx++] = (u8)(chunkLen + 4);
            rec[idx++] = (u8)(addr >> 16);
            rec[idx++] = (u8)(addr >> 8);
            rec[idx++] = (u8)(addr);
            for (u32 i = 0; i < chunkLen; i++)
                rec[idx++] = buf->data[offset + i];
            rec[idx] = srecChecksum(rec, idx);
            fprintf(f, "S2");
            for (int i = 0; i <= idx; i++) fprintf(f, "%02X", rec[i]);
            fprintf(f, "\n");
        }
        offset += chunkLen;
    }

    /* End record */
    fprintf(f, "S9030000FC\n");
    fclose(f);
    return true;
}

/* ── Intel HEX ───────────────────────────────────────── */

bool outputWriteIntelHex(const Buffer *buf, u32 baseAddress, const char *path) {
    if (!buf || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    u32 offset = 0;
    u16 extAddr = 0;

    while (offset < buf->size) {
        u32 addr = baseAddress + offset;
        u16 newExt = (u16)(addr >> 16);
        if (newExt != extAddr) {
            extAddr = newExt;
            u8 cksum = (u8)(0x100 - (2 + 0 + 0 + 4 + (newExt >> 8) + (newExt & 0xFF)));
            fprintf(f, ":02000004%04X%02X\n", newExt, cksum);
        }

        u32 remaining = buf->size - offset;
        u32 chunkLen = remaining > 16 ? 16 : remaining;
        u16 lo = (u16)(addr & 0xFFFF);

        u8 sum = (u8)chunkLen + (u8)(lo >> 8) + (u8)lo + 0x00;
        fprintf(f, ":%02X%04X00", (u8)chunkLen, lo);
        for (u32 i = 0; i < chunkLen; i++) {
            fprintf(f, "%02X", buf->data[offset + i]);
            sum += buf->data[offset + i];
        }
        fprintf(f, "%02X\n", (u8)(0x100 - sum));
        offset += chunkLen;
    }

    fprintf(f, ":00000001FF\n");
    fclose(f);
    return true;
}
