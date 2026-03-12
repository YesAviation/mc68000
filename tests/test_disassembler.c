/* test_disassembler.c — Disassembler tests */
#include "test_framework.h"
#include "common/types.h"
#include "disassembler/disassembler.h"

#include <string.h>

TEST(disasmNop) {
    u8 data[] = { 0x4E, 0x71 };
    char buf[64];
    u32 len = disasmInstruction(data, 0x1000, buf, sizeof(buf));
    ASSERT_EQ(len, 2);
    /* Check that output contains "NOP" */
    ASSERT_TRUE(strstr(buf, "NOP") != NULL);
}

TEST(disasmMoveq) {
    /* MOVEQ #42, D3 → 0x762A */
    u8 data[] = { 0x76, 0x2A };
    char buf[64];
    u32 len = disasmInstruction(data, 0x1000, buf, sizeof(buf));
    ASSERT_EQ(len, 2);
    ASSERT_TRUE(strstr(buf, "MOVEQ") != NULL);
}

TEST(disasmRts) {
    u8 data[] = { 0x4E, 0x75 };
    char buf[64];
    u32 len = disasmInstruction(data, 0x1000, buf, sizeof(buf));
    ASSERT_EQ(len, 2);
    ASSERT_TRUE(strstr(buf, "RTS") != NULL);
}

TEST_SUITE(disassembler) {
    RUN_TEST(disasmNop);
    RUN_TEST(disasmMoveq);
    RUN_TEST(disasmRts);
}

int main(void) {
    RUN_SUITE(disassembler);
    TEST_REPORT();
    return TEST_EXIT_CODE();
}
