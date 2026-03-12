/* ===========================================================================
 *  buffer.h — Dynamic byte buffer (grow-on-demand)
 *  Used for assembler output, ROM loading, etc.
 * =========================================================================== */
#ifndef M68K_BUFFER_H
#define M68K_BUFFER_H

#include "common/types.h"

typedef struct {
    u8    *data;
    u32    size;       /* bytes currently in buffer */
    u32    capacity;   /* allocated capacity */
    u32    position;   /* read cursor */
} Buffer;

/* Create / destroy (heap-allocated) */
Buffer *bufferCreate(u32 initialCapacity);
void    bufferDestroy(Buffer *buf);

/* Init / free (for embedded/stack-allocated Buffer structs) */
void bufferInit(Buffer *buf);
void bufferFree(Buffer *buf);

/* Reset contents but keep allocation */
void bufferClear(Buffer *buf);

/* Write operations (append at end) */
void bufferWriteU8(Buffer *buf, u8 value);
void bufferWriteU16BE(Buffer *buf, u16 value);
void bufferWriteU32BE(Buffer *buf, u32 value);
void bufferWriteBytes(Buffer *buf, const u8 *data, u32 count);

/* Read operations (from current position) */
u8   bufferReadU8(Buffer *buf);
u16  bufferReadU16BE(Buffer *buf);
u32  bufferReadU32BE(Buffer *buf);
void bufferReadBytes(Buffer *buf, u8 *dest, u32 count);

/* Seek / tell */
void bufferSeek(Buffer *buf, u32 position);
u32  bufferTell(const Buffer *buf);

/* Direct access */
u8  *bufferData(const Buffer *buf);
u32  bufferSize(const Buffer *buf);

/* File I/O */
bool bufferLoadFromFile(Buffer *buf, const char *path);
bool bufferSaveToFile(const Buffer *buf, const char *path);

#endif /* M68K_BUFFER_H */
