/* ===========================================================================
 *  cpu.c — MC68000 CPU lifecycle and execution loop
 *
 *  Reset sequence (from the M68000 Users Manual):
 *    1. SSP is loaded from address $000000 (longword)
 *    2. PC  is loaded from address $000004 (longword)
 *    3. SR  is set to $2700 (supervisor mode, IPL=7, all flags clear)
 *    4. Instruction execution begins at the loaded PC
 * =========================================================================== */
#include "cpu/cpu.h"
#include "cpu/decoder.h"
#include "cpu/exceptions.h"
#include "bus/bus.h"
#include "common/log.h"

#include <string.h>

/* ── Lifecycle ── */

void cpuInit(Cpu *cpu, Bus *bus) {
    memset(cpu, 0, sizeof(Cpu));
    cpu->bus = bus;
    cpu->pendingInterruptLevel = -1;

    /* Build the instruction decoder opcode table (once) */
    static bool decoderReady = false;
    if (!decoderReady) {
        decoderInit();
        decoderReady = true;
    }

    /* Give the bus a back-pointer so it can raise address errors */
    if (bus) bus->cpu = cpu;

    LOG_INFO(MOD_CPU, "CPU initialized");
}

void cpuReset(Cpu *cpu) {
    /* Clear all registers */
    memset(cpu->d, 0, sizeof(cpu->d));
    memset(cpu->a, 0, sizeof(cpu->a));

    /* Load initial SSP from vector 0 ($000000) */
    cpu->ssp  = busReadLong(cpu->bus, 0x000000);
    cpu->a[7] = cpu->ssp;

    /* Load initial PC from vector 1 ($000004) */
    cpu->pc = busReadLong(cpu->bus, 0x000004);

    /* Supervisor mode, interrupts masked to level 7 */
    cpu->sr = SR_S | (7 << SR_IPL_SHIFT);

    /* Clear execution state */
    cpu->usp                   = 0;
    cpu->stopped               = false;
    cpu->halted                = false;
    cpu->totalCycles           = 0;
    cpu->instructionCycles     = 0;
    cpu->pendingInterruptLevel = -1;
    cpu->currentOpcode         = 0;
    cpu->currentPC             = cpu->pc;

    LOG_INFO(MOD_CPU, "CPU reset: SSP=$%08X PC=$%08X SR=$%04X",
             cpu->a[7], cpu->pc, cpu->sr);
}

void cpuDestroy(Cpu *cpu) {
    (void)cpu;
    /* Nothing dynamically allocated in Cpu itself */
}

/* ── Supervisor mode switching ── */

void cpuSetSupervisor(Cpu *cpu, bool supervisor) {
    bool wasSupervisor = cpuIsSupervisor(cpu);

    if (supervisor && !wasSupervisor) {
        /* Entering supervisor mode: save USP, load SSP */
        cpu->usp  = cpu->a[7];
        cpu->a[7] = cpu->ssp;
        cpu->sr  |= SR_S;
    } else if (!supervisor && wasSupervisor) {
        /* Leaving supervisor mode: save SSP, load USP */
        cpu->ssp  = cpu->a[7];
        cpu->a[7] = cpu->usp;
        cpu->sr  &= ~SR_S;
    }
}

/* ── Instruction execution ── */

u32 cpuStep(Cpu *cpu) {
    if (cpu->halted) {
        return 4; /* Halted CPU still consumes bus cycles */
    }

    /* Check for pending interrupts (even when stopped) */
    if (cpu->pendingInterruptLevel > 0) {
        int ipl = cpuGetIPL(cpu);
        if (cpu->pendingInterruptLevel > ipl ||
            cpu->pendingInterruptLevel == 7) {
            int level = cpu->pendingInterruptLevel;
            cpu->stopped = false;

            /* Perform IACK cycle: ask bus/devices for vector number */
            int iackResult = busAcknowledgeInterrupt(cpu->bus, level);
            int vector;
            if (iackResult > 0) {
                /* Vectored interrupt: device provided vector number */
                vector = iackResult;
                LOG_DEBUG(MOD_CPU, "Vectored interrupt level %d, vector %d", level, vector);
            } else if (iackResult < 0) {
                /* Autovector: VPA asserted, use autovector formula */
                vector = EXC_VEC_SPURIOUS + level; /* 24 + level = 25..31 */
                LOG_DEBUG(MOD_CPU, "Autovectored interrupt level %d, vector %d", level, vector);
            } else {
                /* Spurious: no device responded */
                vector = EXC_VEC_SPURIOUS; /* vector 24 */
                LOG_DEBUG(MOD_CPU, "Spurious interrupt (level %d)", level);
            }

            exceptionProcess(cpu, vector);
            /* Set IPL mask to the acknowledged level */
            cpuSetIPL(cpu, level);
            /* IACK cycle takes 44 cycles total (already 34 from exceptionProcess) */
            cpuAddCycles(cpu, 10);
        }
    }

    if (cpu->stopped) {
        return 4; /* STOP instruction: wait for interrupt */
    }

    /* Save PC for exception handling */
    cpu->currentPC = cpu->pc;
    cpu->instructionCycles = 0;

    /* Fetch opcode word */
    cpu->currentOpcode = busReadWord(cpu->bus, cpu->pc);
    cpu->pc += 2;

    /* Decode and execute */
    decoderExecute(cpu, cpu->currentOpcode);

    /* Handle trace exception (after instruction completes) */
    if (cpuGetFlag(cpu, SR_T)) {
        exceptionProcess(cpu, EXC_VEC_TRACE);
    }

    cpu->totalCycles += cpu->instructionCycles;
    return cpu->instructionCycles;
}

void cpuRun(Cpu *cpu, u64 targetCycles) {
    u64 start = cpu->totalCycles;

    while ((cpu->totalCycles - start) < targetCycles && !cpu->halted) {
        cpuStep(cpu);
    }
}

/* ── Interrupts ── */

void cpuRaiseInterrupt(Cpu *cpu, int level) {
    if (level >= 1 && level <= 7) {
        if (level > cpu->pendingInterruptLevel) {
            cpu->pendingInterruptLevel = level;
        }
    }
}

void cpuLowerInterrupt(Cpu *cpu, int level) {
    if (cpu->pendingInterruptLevel == level) {
        cpu->pendingInterruptLevel = -1;
    }
}
