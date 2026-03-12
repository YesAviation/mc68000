/* ===========================================================================
 *  buffer.c — Dynamic byte buffer implementation
 * =========================================================================== */
#include "common/buffer.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFFER_DEFAULT_CAP 4096

/* ── Internal: ensure capacity ── */
static void bufferGrow(Buffer *buf, u32 needed) {
    if (buf->size + needed <= buf->capacity) return;

    u32 newCap = buf->capacity * 2;
    while (newCap < buf->size + needed) {
        newCap *= 2;
    }

    u8 *newData = realloc(buf->data, newCap);
    if (!newData) {
        LOG_FATAL(MOD_MAIN, "buffer: out of memory (requested %u bytes)", newCap);
        return;
    }
    buf->data     = newData;
    buf->capacity = newCap;
}

/* ── Create / Destroy ── */

Buffer *bufferCreate(u32 initialCapacity) {
    Buffer *buf = calloc(1, sizeof(Buffer));
    if (!buf) return NULL;

    if (initialCapacity == 0) initialCapacity = BUFFER_DEFAULT_CAP;

    buf->data     = malloc(initialCapacity);
    buf->capacity = initialCapacity;
    buf->size     = 0;
    buf->position = 0;

    if (!buf->data) {
        free(buf);
        return NULL;
    }
    return buf;
}

void bufferDestroy(Buffer *buf) {
    if (!buf) return;
    free(buf->data);
    free(buf);
}

/* Init/Free for embedded (non-heap) Buffer structs */
void bufferInit(Buffer *buf) {
    buf->data     = malloc(BUFFER_DEFAULT_CAP);
    buf->capacity = BUFFER_DEFAULT_CAP;
    buf->size     = 0;
    buf->position = 0;
}

void bufferFree(Buffer *buf) {
    if (!buf) return;
    free(buf->data);
    buf->data     = NULL;
    buf->capacity = 0;
    buf->size     = 0;
    buf->position = 0;
}

void bufferClear(Buffer *buf) {
    if (!buf) return;
    buf->size     = 0;
    buf->position = 0;
}

/* ── Write ── */

void bufferWriteU8(Buffer *buf, u8 value) {
    bufferGrow(buf, 1);
    buf->data[buf->size++] = value;
}

void bufferWriteU16BE(Buffer *buf, u16 value) {
    bufferGrow(buf, 2);
    buf->data[buf->size++] = (u8)(value >> 8);
    buf->data[buf->size++] = (u8)(value & 0xFF);
}

void bufferWriteU32BE(Buffer *buf, u32 value) {
    bufferGrow(buf, 4);
    buf->data[buf->size++] = (u8)(value >> 24);
    buf->data[buf->size++] = (u8)(value >> 16);
    buf->data[buf->size++] = (u8)(value >> 8);
    buf->data[buf->size++] = (u8)(value & 0xFF);
}

void bufferWriteBytes(Buffer *buf, const u8 *data, u32 count) {
    bufferGrow(buf, count);
    memcpy(buf->data + buf->size, data, count);
    buf->size += count;
}

/* ── Read ── */

u8 bufferReadU8(Buffer *buf) {
    if (buf->position >= buf->size) return 0;
    return buf->data[buf->position++];
}

u16 bufferReadU16BE(Buffer *buf) {
    if (buf->position + 2 > buf->size) return 0;
    u16 val = (u16)((buf->data[buf->position] << 8) |
                     buf->data[buf->position + 1]);
    buf->position += 2;
    return val;
}

u32 bufferReadU32BE(Buffer *buf) {
    if (buf->position + 4 > buf->size) return 0;
    u32 val = ((u32)buf->data[buf->position]     << 24) |
              ((u32)buf->data[buf->position + 1] << 16) |
              ((u32)buf->data[buf->position + 2] << 8)  |
              ((u32)buf->data[buf->position + 3]);
    buf->position += 4;
    return val;
}

void bufferReadBytes(Buffer *buf, u8 *dest, u32 count) {
    u32 avail = buf->size - buf->position;
    if (count > avail) count = avail;
    memcpy(dest, buf->data + buf->position, count);
    buf->position += count;
}

/* ── Seek / Tell ── */

void bufferSeek(Buffer *buf, u32 position) {
    if (position > buf->size) position = buf->size;
    buf->position = position;
}

u32 bufferTell(const Buffer *buf) {
    return buf->position;
}

/* ── Direct access ── */

u8 *bufferData(const Buffer *buf) {
    return buf->data;
}

u32 bufferSize(const Buffer *buf) {
    return buf->size;
}

/* ── File I/O ── */

bool bufferLoadFromFile(Buffer *buf, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize < 0) {
        fclose(f);
        return false;
    }

    bufferClear(buf);
    bufferGrow(buf, (u32)fileSize);

    size_t bytesRead = fread(buf->data, 1, (size_t)fileSize, f);
    buf->size = (u32)bytesRead;
    fclose(f);

    return bytesRead == (size_t)fileSize;
}

bool bufferSaveToFile(const Buffer *buf, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t written = fwrite(buf->data, 1, buf->size, f);
    fclose(f);

    return written == buf->size;
}
