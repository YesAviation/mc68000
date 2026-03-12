/* registers.c — Register allocator */
#include "compiler/backend/registers.h"
#include <string.h>

static const char *regNames[REG_TOTAL] = {
    "D0","D1","D2","D3","D4","D5","D6","D7",
    "A0","A1","A2","A3","A4","A5","A6","A7"
};

void regAllocInit(RegAllocator *ra) {
    memset(ra, 0, sizeof(*ra));
    memset(ra->tempMap, -1, sizeof(ra->tempMap));
    /* A6 = FP, A7 = SP — always reserved */
    ra->inUse[REG_A6] = true;
    ra->inUse[REG_A7] = true;
}

M68kReg regAllocData(RegAllocator *ra) {
    /* Prefer scratch registers first, then callee-saved */
    static const int order[] = { REG_D0, REG_D1, REG_D2, REG_D3, REG_D4, REG_D5, REG_D6, REG_D7 };
    for (int i = 0; i < 8; i++) {
        if (!ra->inUse[order[i]]) {
            ra->inUse[order[i]] = true;
            return (M68kReg)order[i];
        }
    }
    ra->spillCount++;
    return REG_NONE;
}

M68kReg regAllocAddr(RegAllocator *ra) {
    static const int order[] = { REG_A0, REG_A1, REG_A2, REG_A3, REG_A4, REG_A5 };
    for (int i = 0; i < 6; i++) {
        if (!ra->inUse[order[i]]) {
            ra->inUse[order[i]] = true;
            return (M68kReg)order[i];
        }
    }
    ra->spillCount++;
    return REG_NONE;
}

void regFree(RegAllocator *ra, M68kReg reg) {
    if (reg >= 0 && reg < REG_TOTAL && reg != REG_A6 && reg != REG_A7)
        ra->inUse[reg] = false;
}

bool regIsCallerSaved(M68kReg reg) {
    return reg == REG_D0 || reg == REG_D1 || reg == REG_A0 || reg == REG_A1;
}

const char *regName(M68kReg reg) {
    if (reg >= 0 && reg < REG_TOTAL) return regNames[reg];
    return "???";
}
