/* registers.h — Register allocator for MC68000 */
#ifndef M68K_CC_REGISTERS_H
#define M68K_CC_REGISTERS_H

#include "common/types.h"

/*
 * MC68000 register file for code generation:
 *   D0-D1:  scratch / return value (caller-saved)
 *   D2-D7:  general purpose (callee-saved)
 *   A0-A1:  scratch / address (caller-saved)
 *   A2-A5:  general purpose address (callee-saved)
 *   A6:     frame pointer
 *   A7:     stack pointer
 */

typedef enum {
    REG_D0 = 0, REG_D1, REG_D2, REG_D3, REG_D4, REG_D5, REG_D6, REG_D7,
    REG_A0, REG_A1, REG_A2, REG_A3, REG_A4, REG_A5, REG_A6, REG_A7,
    REG_NONE = -1
} M68kReg;

#define REG_DATA_COUNT   8
#define REG_ADDR_COUNT   8
#define REG_TOTAL       16

typedef struct {
    bool inUse[REG_TOTAL];
    int  tempMap[256];  /* IR temp → physical register (-1 = spilled) */
    int  spillCount;
} RegAllocator;

void regAllocInit(RegAllocator *ra);

/* Allocate a data register; returns REG_NONE if all in use (spill needed) */
M68kReg regAllocData(RegAllocator *ra);
M68kReg regAllocAddr(RegAllocator *ra);

void regFree(RegAllocator *ra, M68kReg reg);
bool regIsCallerSaved(M68kReg reg);
const char *regName(M68kReg reg);

#endif /* M68K_CC_REGISTERS_H */
