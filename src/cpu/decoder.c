/* ===========================================================================
 *  decoder.c — Instruction decoder implementation
 * =========================================================================== */
#include "cpu/decoder.h"
#include "cpu/opcodes.h"
#include "cpu/exceptions.h"
#include "common/log.h"

/* ── The 64K opcode table ── */
OpcodeEntry opcodeTable[65536];

/* ── Default handler for unimplemented / illegal opcodes ── */
static void handleIllegal(Cpu *cpu) {
    LOG_WARN(MOD_CPU, "Illegal instruction $%04X at PC=$%06X",
             cpu->currentOpcode, cpu->currentPC);
    exceptionProcess(cpu, EXC_VEC_ILLEGAL_INSTRUCTION);
    cpuAddCycles(cpu, 34);
}

/* ── Line-A emulator trap ── */
static void handleLineA(Cpu *cpu) {
    LOG_WARN(MOD_CPU, "Line-A emulator trap $%04X at PC=$%06X",
             cpu->currentOpcode, cpu->currentPC);
    exceptionProcess(cpu, EXC_VEC_LINE_A);
    cpuAddCycles(cpu, 34);
}

/* ── Line-F emulator trap ── */
static void handleLineF(Cpu *cpu) {
    LOG_WARN(MOD_CPU, "Line-F emulator trap $%04X at PC=$%06X",
             cpu->currentOpcode, cpu->currentPC);
    exceptionProcess(cpu, EXC_VEC_LINE_F);
    cpuAddCycles(cpu, 34);
}

/* ── Build the opcode table ── */
void decoderInit(void) {
    /* Fill entire table with illegal instruction handler */
    for (int i = 0; i < 65536; i++) {
        opcodeTable[i].handler    = handleIllegal;
        opcodeTable[i].mnemonic   = "ILLEGAL";
        opcodeTable[i].baseCycles = 34;
    }

    /* Override Line-A ($Axxx) range */
    for (int i = 0xA000; i <= 0xAFFF; i++) {
        opcodeTable[i].handler  = handleLineA;
        opcodeTable[i].mnemonic = "LINE_A";
    }

    /* Override Line-F ($Fxxx) range */
    for (int i = 0xF000; i <= 0xFFFF; i++) {
        opcodeTable[i].handler  = handleLineF;
        opcodeTable[i].mnemonic = "LINE_F";
    }

    /* Register all instruction handlers */
    opcodesRegisterAll();

    LOG_INFO(MOD_CPU, "Decoder initialized: 65536 opcode entries built");
}

/* ── Execute ── */
void decoderExecute(Cpu *cpu, u16 opcode) {
    opcodeTable[opcode].handler(cpu);
}
