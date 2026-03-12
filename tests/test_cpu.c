/* test_cpu.c — CPU core tests */
#include "test_framework.h"
#include "common/types.h"
#include "cpu/cpu.h"
#include "cpu/decoder.h"
#include "cpu/alu.h"
#include "cpu/exceptions.h"
#include "bus/bus.h"
#include "bus/memory.h"

/* Helper: create a minimal machine for testing */
static Bus    testBus;
static Cpu    testCpu;

static void setupCpu(void) {
    busInit(&testBus, 0x100000, 0x10000, 0xF00000);

    /* Write initial vectors: SSP=$00010000, PC=$00001000 */
    busWriteLong(&testBus, 0x000000, 0x00010000);
    busWriteLong(&testBus, 0x000004, 0x00001000);

    cpuInit(&testCpu, &testBus);
    decoderInit();
    cpuReset(&testCpu);
}

/* Helper: write instruction words at the CPU's current PC */
static void writeInstr(u32 addr, u16 word) {
    busWriteWord(&testBus, addr, word);
}

/* ── Register init after reset ──────────────────────── */
TEST(cpuResetState) {
    setupCpu();
    ASSERT_HEX_EQ(testCpu.a[7], 0x00010000);  /* SSP loaded from vector 0 */
    ASSERT_HEX_EQ(testCpu.pc,   0x00001000);  /* PC loaded from vector 1 */
    ASSERT_TRUE((testCpu.sr & 0x2000) != 0);   /* Supervisor mode */
    ASSERT_TRUE((testCpu.sr & 0x0700) == 0x0700); /* IPL = 7 */
    ASSERT_FALSE(testCpu.halted);
    ASSERT_FALSE(testCpu.stopped);
}

/* ── NOP instruction ────────────────────────────────── */
TEST(cpuNop) {
    setupCpu();
    writeInstr(0x1000, 0x4E71); /* NOP */
    u32 pcBefore = testCpu.pc;
    int cycles = cpuStep(&testCpu);
    ASSERT_HEX_EQ(testCpu.pc, pcBefore + 2);
    ASSERT_TRUE(cycles > 0);
}

/* ── MOVEQ ──────────────────────────────────────────── */
TEST(cpuMoveq) {
    setupCpu();
    /* MOVEQ #42, D3  → 0x7642 (0111 011 0 00101010) */
    writeInstr(0x1000, 0x762A);
    cpuStep(&testCpu);
    ASSERT_EQ(testCpu.d[3], 42);
}

TEST(cpuMoveqNegative) {
    setupCpu();
    /* MOVEQ #-1, D0  → 0x70FF */
    writeInstr(0x1000, 0x70FF);
    cpuStep(&testCpu);
    ASSERT_HEX_EQ(testCpu.d[0], 0xFFFFFFFF); /* sign-extended to 32 bits */
}

/* ── ALU unit tests ─────────────────────────────────── */
TEST(aluAddWord) {
    setupCpu();
    u32 result = aluAdd(&testCpu, 0x7FFF, 0x0001, SIZE_WORD);
    ASSERT_HEX_EQ(result & 0xFFFF, 0x8000);
    ASSERT_TRUE(testCpu.sr & SR_V);  /* Overflow: pos + pos = neg */
    ASSERT_TRUE(testCpu.sr & SR_N);  /* Negative */
    ASSERT_FALSE(testCpu.sr & SR_Z); /* Not zero */
}

TEST(aluSubByte) {
    setupCpu();
    u32 result = aluSub(&testCpu, 0x01, 0x00, SIZE_BYTE);
    ASSERT_HEX_EQ(result & 0xFF, 0xFF);
    ASSERT_TRUE(testCpu.sr & SR_C);  /* Borrow */
    ASSERT_TRUE(testCpu.sr & SR_N);  /* Negative */
}

TEST(aluAndLong) {
    setupCpu();
    u32 result = aluAnd(&testCpu, 0xFF00FF00, 0x0F0F0F0F, SIZE_LONG);
    ASSERT_HEX_EQ(result, 0x0F000F00);
    ASSERT_FALSE(testCpu.sr & SR_Z);
}

TEST(aluOrZero) {
    setupCpu();
    u32 result = aluOr(&testCpu, 0x0000, 0x0000, SIZE_WORD);
    ASSERT_HEX_EQ(result, 0);
    ASSERT_TRUE(testCpu.sr & SR_Z);  /* Zero */
}

/* ── Bus read/write ─────────────────────────────────── */
TEST(busReadWriteByte) {
    setupCpu();
    busWriteByte(&testBus, 0x100, 0xAB);
    ASSERT_HEX_EQ(busReadByte(&testBus, 0x100), 0xAB);
}

TEST(busReadWriteWord) {
    setupCpu();
    busWriteWord(&testBus, 0x200, 0xDEAD);
    ASSERT_HEX_EQ(busReadWord(&testBus, 0x200), 0xDEAD);
}

TEST(busReadWriteLong) {
    setupCpu();
    busWriteLong(&testBus, 0x300, 0xCAFEBABE);
    ASSERT_HEX_EQ(busReadLong(&testBus, 0x300), 0xCAFEBABE);
}

TEST(busBigEndian) {
    setupCpu();
    busWriteWord(&testBus, 0x400, 0x1234);
    ASSERT_HEX_EQ(busReadByte(&testBus, 0x400), 0x12);  /* High byte first */
    ASSERT_HEX_EQ(busReadByte(&testBus, 0x401), 0x34);  /* Low byte second */
}

/* ── Exception vector table ──────────────────────────── */
TEST(exceptionVectors) {
    ASSERT_EQ(EXC_VEC_RESET_SSP, 0);
    ASSERT_EQ(EXC_VEC_RESET_PC,  1);
    ASSERT_EQ(EXC_VEC_BUS_ERROR, 2);
    ASSERT_EQ(EXC_VEC_ADDRESS_ERROR, 3);
    ASSERT_EQ(EXC_VEC_ILLEGAL_INSTRUCTION, 4);
    ASSERT_EQ(EXC_VEC_TRAP_0, 32);
}

/* ── Suite ───────────────────────────────────────────── */
TEST_SUITE(cpu) {
    RUN_TEST(cpuResetState);
    RUN_TEST(cpuNop);
    RUN_TEST(cpuMoveq);
    RUN_TEST(cpuMoveqNegative);
    RUN_TEST(aluAddWord);
    RUN_TEST(aluSubByte);
    RUN_TEST(aluAndLong);
    RUN_TEST(aluOrZero);
    RUN_TEST(busReadWriteByte);
    RUN_TEST(busReadWriteWord);
    RUN_TEST(busReadWriteLong);
    RUN_TEST(busBigEndian);
    RUN_TEST(exceptionVectors);
}

int main(void) {
    RUN_SUITE(cpu);
    TEST_REPORT();
    return TEST_EXIT_CODE();
}
