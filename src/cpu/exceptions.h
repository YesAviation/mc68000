/* ===========================================================================
 *  exceptions.h — MC68000 exception processing
 *
 *  Exception vector table (address = vector number * 4):
 *    Vec  Address  Name
 *    ───  ───────  ────
 *    0    $000     Reset: Initial SSP
 *    1    $004     Reset: Initial PC
 *    2    $008     Bus Error
 *    3    $00C     Address Error
 *    4    $010     Illegal Instruction
 *    5    $014     Zero Divide
 *    6    $018     CHK Instruction
 *    7    $01C     TRAPV Instruction
 *    8    $020     Privilege Violation
 *    9    $024     Trace
 *    10   $028     Line-A Emulator (1010)
 *    11   $02C     Line-F Emulator (1111)
 *    12-14         (Reserved)
 *    15   $03C     Uninitialized Interrupt Vector
 *    16-23         (Reserved)
 *    24   $060     Spurious Interrupt
 *    25   $064     Level 1 Interrupt Autovector
 *    26   $068     Level 2 Interrupt Autovector
 *    27   $06C     Level 3 Interrupt Autovector
 *    28   $070     Level 4 Interrupt Autovector
 *    29   $074     Level 5 Interrupt Autovector
 *    30   $078     Level 6 Interrupt Autovector
 *    31   $07C     Level 7 Interrupt Autovector (NMI)
 *    32-47         TRAP #0 – TRAP #15
 *    48-63         (Reserved)
 *    64-255        User Interrupt Vectors
 * =========================================================================== */
#ifndef M68K_EXCEPTIONS_H
#define M68K_EXCEPTIONS_H

#include "cpu/cpu.h"

/* ── Exception vector numbers ── */
#define EXC_VEC_RESET_SSP              0
#define EXC_VEC_RESET_PC               1
#define EXC_VEC_BUS_ERROR              2
#define EXC_VEC_ADDRESS_ERROR          3
#define EXC_VEC_ILLEGAL_INSTRUCTION    4
#define EXC_VEC_ZERO_DIVIDE            5
#define EXC_VEC_CHK                    6
#define EXC_VEC_TRAPV                  7
#define EXC_VEC_PRIVILEGE_VIOLATION    8
#define EXC_VEC_TRACE                  9
#define EXC_VEC_LINE_A                 10
#define EXC_VEC_LINE_F                 11
#define EXC_VEC_UNINITIALIZED_INT      15
#define EXC_VEC_SPURIOUS               24
#define EXC_VEC_AUTO_1                 25
#define EXC_VEC_AUTO_2                 26
#define EXC_VEC_AUTO_3                 27
#define EXC_VEC_AUTO_4                 28
#define EXC_VEC_AUTO_5                 29
#define EXC_VEC_AUTO_6                 30
#define EXC_VEC_AUTO_7                 31
#define EXC_VEC_TRAP_0                 32
#define EXC_VEC_TRAP_15                47

/* Total vectors in the table */
#define EXC_VECTOR_COUNT               256

/* Address of a vector: vector number * 4 */
#define EXC_VECTOR_ADDR(vec)           ((u32)(vec) * 4)

/* ── Exception processing ──
 *  1. Save SR
 *  2. Enter supervisor mode (set S bit)
 *  3. Push PC and saved SR onto supervisor stack
 *  4. Clear trace flag
 *  5. Load PC from exception vector
 *  6. For interrupts: set IPL in SR to interrupt level
 */
void exceptionProcess(Cpu *cpu, int vector);

/* ── Specific exception generators ── */
void exceptionBusError(Cpu *cpu, u32 address, bool isWrite, bool isInstruction);
void exceptionAddressError(Cpu *cpu, u32 address, bool isWrite, bool isInstruction);
void exceptionIllegalInstruction(Cpu *cpu);
void exceptionZeroDivide(Cpu *cpu);
void exceptionChk(Cpu *cpu);
void exceptionTrapV(Cpu *cpu);
void exceptionPrivilegeViolation(Cpu *cpu);
void exceptionTrace(Cpu *cpu);
void exceptionTrap(Cpu *cpu, int trapNumber);      /* TRAP #0-#15 */
void exceptionInterrupt(Cpu *cpu, int level);       /* Autovectored interrupt */

#endif /* M68K_EXCEPTIONS_H */
