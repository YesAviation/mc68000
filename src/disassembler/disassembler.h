/* disassembler.h — MC68000 disassembler */
#ifndef M68K_DISASSEMBLER_H
#define M68K_DISASSEMBLER_H

#include "common/types.h"

/*
 * Disassembles MC68000 machine code into Motorola-syntax assembly.
 *
 * Usage:
 *   u32 addr = 0x1000;
 *   while (addr < end) {
 *       char buf[128];
 *       u32 len = disasmInstruction(data + (addr - base), addr, buf, sizeof(buf));
 *       printf("%06X  %s\n", addr, buf);
 *       addr += len;
 *   }
 */

/* Disassemble a single instruction.
 * Returns the number of bytes consumed (2, 4, 6, 8, or 10). */
u32 disasmInstruction(const u8 *code, u32 address, char *outBuf, int outBufSize);

/* Disassemble a range and write to file */
bool disasmRange(const u8 *code, u32 baseAddress, u32 length, const char *outPath);

/* Helper: format a single opcode word for hex dump */
void disasmFormatHex(const u8 *code, u32 length, char *outBuf, int outBufSize);

#endif /* M68K_DISASSEMBLER_H */
