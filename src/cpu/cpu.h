/* ===========================================================================
 *  cpu.h — MC68000 CPU state and public API
 *
 *  This is the central data structure for the entire emulator.
 *  The MC68000 has:
 *    - 8 data registers (D0-D7), 32-bit
 *    - 8 address registers (A0-A7), 32-bit
 *    - Two stack pointers: USP (user) and SSP (supervisor)
 *      A7 is the active SP; the other is shadowed.
 *    - 32-bit Program Counter (only 24 bits on the bus)
 *    - 16-bit Status Register (CCR in low byte, system byte in high byte)
 * =========================================================================== */
#ifndef M68K_CPU_H
#define M68K_CPU_H

#include "common/types.h"

/* Forward declaration */
typedef struct Bus Bus;

/* ── Status Register bit definitions ── */

/* Condition Code Register (lower 8 bits of SR) */
#define SR_C        BIT(0)   /* Carry        */
#define SR_V        BIT(1)   /* Overflow     */
#define SR_Z        BIT(2)   /* Zero         */
#define SR_N        BIT(3)   /* Negative     */
#define SR_X        BIT(4)   /* Extend       */

/* System byte (upper 8 bits of SR) */
#define SR_IPL_MASK  0x0700  /* Interrupt Priority Level (bits 10-8) */
#define SR_IPL_SHIFT 8
#define SR_S         BIT(13) /* Supervisor mode */
#define SR_T         BIT(15) /* Trace mode      */

/* ── CPU state ── */
typedef struct Cpu {
    /* === Programmer-visible registers === */
    u32  d[8];          /* D0-D7  data registers          */
    u32  a[8];          /* A0-A7  address registers        */
                        /* a[7] = currently active SP      */
    u32  usp;           /* User Stack Pointer (shadow)     */
    u32  ssp;           /* Supervisor Stack Pointer (shadow) */
    u32  pc;            /* Program Counter                 */
    u16  sr;            /* Status Register                 */

    /* === Internal execution state === */
    bool stopped;       /* Set by STOP instruction, cleared by interrupt  */
    bool halted;        /* Set on double bus fault — CPU is dead          */
    u16  currentOpcode; /* Opcode word being executed                     */
    u32  currentPC;     /* PC at start of current instruction             */
    u64  totalCycles;   /* Total cycle count since reset                  */
    u32  instructionCycles; /* Cycles consumed by current instruction     */

    /* === Interrupt state === */
    s32  pendingInterruptLevel; /* -1 = none, 1-7 = level                */

    /* === Bus connection === */
    Bus *bus;
} Cpu;

/* ── Public API ── */

/* Lifecycle */
void cpuInit(Cpu *cpu, Bus *bus);
void cpuReset(Cpu *cpu);
void cpuDestroy(Cpu *cpu);

/* Execution */
u32  cpuStep(Cpu *cpu);              /* Execute one instruction, return cycles */
void cpuRun(Cpu *cpu, u64 cycles);   /* Run for N cycles                      */

/* Interrupts */
void cpuRaiseInterrupt(Cpu *cpu, int level);  /* Assert interrupt (1-7) */
void cpuLowerInterrupt(Cpu *cpu, int level);  /* Deassert interrupt     */

/* ── SR helpers ── */

static inline bool cpuGetFlag(const Cpu *cpu, u16 flag) {
    return (cpu->sr & flag) != 0;
}

static inline void cpuSetFlag(Cpu *cpu, u16 flag, bool value) {
    if (value)
        cpu->sr |= flag;
    else
        cpu->sr &= ~flag;
}

static inline bool cpuIsSupervisor(const Cpu *cpu) {
    return (cpu->sr & SR_S) != 0;
}

static inline int cpuGetIPL(const Cpu *cpu) {
    return (cpu->sr & SR_IPL_MASK) >> SR_IPL_SHIFT;
}

static inline void cpuSetIPL(Cpu *cpu, int level) {
    cpu->sr = (cpu->sr & ~SR_IPL_MASK) | ((level & 7) << SR_IPL_SHIFT);
}

/* Switch between user and supervisor mode (swaps A7 with shadow SP) */
void cpuSetSupervisor(Cpu *cpu, bool supervisor);

/* ── Register access helpers ── */

/* Read a data register masked to the operation size */
static inline u32 cpuReadD(const Cpu *cpu, int reg, OperationSize sz) {
    return cpu->d[reg] & sizeMask(sz);
}

/* Write to a data register, preserving upper bits for byte/word */
static inline void cpuWriteD(Cpu *cpu, int reg, u32 value, OperationSize sz) {
    u32 mask = sizeMask(sz);
    cpu->d[reg] = (cpu->d[reg] & ~mask) | (value & mask);
}

/* Read an address register (always full 32-bit) */
static inline u32 cpuReadA(const Cpu *cpu, int reg) {
    return cpu->a[reg];
}

/* Write an address register (always full 32-bit, even for word ops = sign-extend) */
static inline void cpuWriteA(Cpu *cpu, int reg, u32 value) {
    cpu->a[reg] = value;
}

/* Add cycles to the instruction cycle counter */
static inline void cpuAddCycles(Cpu *cpu, u32 cycles) {
    cpu->instructionCycles += cycles;
}

#endif /* M68K_CPU_H */
