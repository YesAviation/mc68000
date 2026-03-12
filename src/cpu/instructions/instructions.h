/* ===========================================================================
 *  instructions.h — Common header for all instruction implementations
 * =========================================================================== */
#ifndef M68K_INSTRUCTIONS_H
#define M68K_INSTRUCTIONS_H

#include "cpu/cpu.h"
#include "cpu/decoder.h"
#include "cpu/addressing.h"
#include "cpu/alu.h"
#include "cpu/exceptions.h"
#include "cpu/timing.h"
#include "bus/bus.h"
#include "common/log.h"

/* Fetch extension word from (PC) and advance PC */
static inline u16 instrFetchWord(Cpu *cpu) {
    u16 w = busReadWord(cpu->bus, cpu->pc);
    cpu->pc += 2;
    return w;
}

/* Fetch extension long from (PC) and advance PC */
static inline u32 instrFetchLong(Cpu *cpu) {
    u32 hi = (u32)busReadWord(cpu->bus, cpu->pc) << 16;
    cpu->pc += 2;
    u32 lo = (u32)busReadWord(cpu->bus, cpu->pc);
    cpu->pc += 2;
    return hi | lo;
}

#endif /* M68K_INSTRUCTIONS_H */
