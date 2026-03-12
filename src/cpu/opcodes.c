/* ===========================================================================
 *  opcodes.c — Master opcode registration
 * =========================================================================== */
#include "cpu/opcodes.h"
#include "common/log.h"

void opcodesRegisterAll(void) {
    LOG_DEBUG(MOD_CPU, "Registering all opcodes...");

    opcodesRegisterMove();
    opcodesRegisterArithmetic();
    opcodesRegisterLogic();
    opcodesRegisterShift();
    opcodesRegisterBranch();
    opcodesRegisterBcd();
    opcodesRegisterSystem();
    opcodesRegisterBit();

    /* Count how many valid (non-ILLEGAL) opcodes we registered */
    int count = 0;
    for (int i = 0; i < 65536; i++) {
        if (opcodeTable[i].mnemonic[0] != 'I' &&  /* not "ILLEGAL" */
            opcodeTable[i].mnemonic[0] != 'L') {   /* not "LINE_A/F" */
            count++;
        }
    }
    LOG_INFO(MOD_CPU, "Registered %d valid opcode patterns", count);
}
