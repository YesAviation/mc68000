/* output.h — Binary output formats */
#ifndef M68K_ASM_OUTPUT_H
#define M68K_ASM_OUTPUT_H

#include "common/types.h"
#include "common/buffer.h"

/* Write raw binary */
bool outputWriteBinary(const Buffer *buf, const char *path);

/* Write Motorola S-record (S19) format */
bool outputWriteSRecord(const Buffer *buf, u32 baseAddress, const char *path);

/* Write Intel HEX format */
bool outputWriteIntelHex(const Buffer *buf, u32 baseAddress, const char *path);

#endif /* M68K_ASM_OUTPUT_H */
