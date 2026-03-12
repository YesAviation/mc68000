/* ===========================================================================
 *  exceptions.c — MC68000 exception processing
 * =========================================================================== */
#include "cpu/exceptions.h"
#include "bus/bus.h"
#include "common/log.h"

/* ── Push helpers (push onto supervisor stack) ── */
static void pushWord(Cpu *cpu, u16 value) {
    cpu->a[7] -= 2;
    busWriteWord(cpu->bus, cpu->a[7] & ADDR_MASK, value);
}

static void pushLong(Cpu *cpu, u32 value) {
    cpu->a[7] -= 4;
    busWriteLong(cpu->bus, cpu->a[7] & ADDR_MASK, value);
}

/* ── Generic exception processing ── */
void exceptionProcess(Cpu *cpu, int vector) {
    /* 1. Save current SR */
    u16 savedSR = cpu->sr;

    /* 2. Enter supervisor mode */
    cpuSetSupervisor(cpu, true);

    /* 3. Clear trace flag */
    cpuSetFlag(cpu, SR_T, false);

    /* 4. For interrupts, update IPL mask */
    if (vector >= EXC_VEC_AUTO_1 && vector <= EXC_VEC_AUTO_7) {
        int level = vector - EXC_VEC_SPURIOUS;
        cpuSetIPL(cpu, level);
    }

    /* 5. Push PC and SR onto supervisor stack */
    pushLong(cpu, cpu->pc);
    pushWord(cpu, savedSR);

    /* 6. Load new PC from vector table */
    u32 vectorAddr = EXC_VECTOR_ADDR(vector);
    u32 newPC = busReadLong(cpu->bus, vectorAddr);
    cpu->pc = newPC & ADDR_MASK;

    LOG_DEBUG(MOD_CPU, "Exception vector %d: PC=$%06X -> $%06X (SR=$%04X -> $%04X)",
              vector, cpu->currentPC, cpu->pc, savedSR, cpu->sr);

    /* Base exception processing cost */
    cpuAddCycles(cpu, 34);
}

/* ── Bus error: pushes additional information (group 0 exception) ──
 *  Stack frame for bus/address error (from top):
 *    SR, PC (high), PC (low), IR, Access Address, R/W | I/N | FC */
void exceptionBusError(Cpu *cpu, u32 address, bool isWrite, bool isInstruction) {
    u16 savedSR = cpu->sr;
    cpuSetSupervisor(cpu, true);
    cpuSetFlag(cpu, SR_T, false);

    /* Build the special stack frame for group 0 exceptions */
    u16 info = 0;
    if (!isWrite)       info |= 0x10;  /* R/W: 1=read, 0=write */
    if (!isInstruction) info |= 0x08;  /* I/N: 1=not instruction, 0=instruction */
    /* Function code in bits 2-0 (simplified) */
    info |= cpuIsSupervisor(cpu) ? 0x05 : 0x01;

    /* Push in order: info word, access address, IR, SR, PC */
    pushWord(cpu, info);
    pushLong(cpu, address);
    pushWord(cpu, cpu->currentOpcode);
    pushWord(cpu, savedSR);
    pushLong(cpu, cpu->pc);

    /* If we get a bus error during bus error processing, halt */
    static bool inBusError = false;
    if (inBusError) {
        LOG_FATAL(MOD_CPU, "Double bus fault at $%06X — CPU halted", cpu->currentPC);
        cpu->halted = true;
        inBusError = false;
        return;
    }
    inBusError = true;

    u32 newPC = busReadLong(cpu->bus, EXC_VECTOR_ADDR(EXC_VEC_BUS_ERROR));
    cpu->pc = newPC & ADDR_MASK;
    inBusError = false;

    cpuAddCycles(cpu, 50);
}

void exceptionAddressError(Cpu *cpu, u32 address, bool isWrite, bool isInstruction) {
    /* Address errors use the same stack frame as bus errors */
    u16 savedSR = cpu->sr;
    cpuSetSupervisor(cpu, true);
    cpuSetFlag(cpu, SR_T, false);

    u16 info = 0;
    if (!isWrite)       info |= 0x10;
    if (!isInstruction) info |= 0x08;
    info |= cpuIsSupervisor(cpu) ? 0x05 : 0x01;

    pushWord(cpu, info);
    pushLong(cpu, address);
    pushWord(cpu, cpu->currentOpcode);
    pushWord(cpu, savedSR);
    pushLong(cpu, cpu->pc);

    u32 newPC = busReadLong(cpu->bus, EXC_VECTOR_ADDR(EXC_VEC_ADDRESS_ERROR));
    cpu->pc = newPC & ADDR_MASK;

    cpuAddCycles(cpu, 50);
}

void exceptionIllegalInstruction(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_ILLEGAL_INSTRUCTION);
}

void exceptionZeroDivide(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_ZERO_DIVIDE);
    cpuAddCycles(cpu, 4); /* Additional cycles for DIVU/DIVS trap */
}

void exceptionChk(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_CHK);
    cpuAddCycles(cpu, 6);
}

void exceptionTrapV(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_TRAPV);
}

void exceptionPrivilegeViolation(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_PRIVILEGE_VIOLATION);
}

void exceptionTrace(Cpu *cpu) {
    exceptionProcess(cpu, EXC_VEC_TRACE);
}

void exceptionTrap(Cpu *cpu, int trapNumber) {
    if (trapNumber < 0 || trapNumber > 15) return;
    exceptionProcess(cpu, EXC_VEC_TRAP_0 + trapNumber);
}

void exceptionInterrupt(Cpu *cpu, int level) {
    if (level < 1 || level > 7) return;
    exceptionProcess(cpu, EXC_VEC_SPURIOUS + level);
}
