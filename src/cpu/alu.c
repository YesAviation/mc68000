/* ===========================================================================
 *  alu.c — ALU operations and condition code computation
 * =========================================================================== */
#include "cpu/alu.h"

/* ── Condition code evaluation ── */
bool aluTestCondition(const Cpu *cpu, ConditionCode cc) {
    bool c = cpuGetFlag(cpu, SR_C);
    bool v = cpuGetFlag(cpu, SR_V);
    bool z = cpuGetFlag(cpu, SR_Z);
    bool n = cpuGetFlag(cpu, SR_N);

    switch (cc) {
        case COND_T:  return true;
        case COND_F:  return false;
        case COND_HI: return !c && !z;
        case COND_LS: return c || z;
        case COND_CC: return !c;
        case COND_CS: return c;
        case COND_NE: return !z;
        case COND_EQ: return z;
        case COND_VC: return !v;
        case COND_VS: return v;
        case COND_PL: return !n;
        case COND_MI: return n;
        case COND_GE: return (n && v) || (!n && !v);
        case COND_LT: return (n && !v) || (!n && v);
        case COND_GT: return (n && v && !z) || (!n && !v && !z);
        case COND_LE: return z || (n && !v) || (!n && v);
    }
    return false;
}

/* ── Flag helpers ── */

void aluSetNZ(Cpu *cpu, u32 result, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    result &= mask;

    cpuSetFlag(cpu, SR_N, (result & msb) != 0);
    cpuSetFlag(cpu, SR_Z, result == 0);
}

void aluSetLogicFlags(Cpu *cpu, u32 result, OperationSize sz) {
    aluSetNZ(cpu, result, sz);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
}

void aluSetClrFlags(Cpu *cpu) {
    cpuSetFlag(cpu, SR_N, false);
    cpuSetFlag(cpu, SR_Z, true);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
}

void aluSetTstFlags(Cpu *cpu, u32 value, OperationSize sz) {
    aluSetLogicFlags(cpu, value, sz);
}

/* ── Addition ── */
u32 aluAdd(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);

    src &= mask;
    dst &= mask;
    u32 result = (src + dst) & mask;

    /* Carry: set if unsigned overflow */
    bool carry = (u64)src + (u64)dst > mask;

    /* Overflow: set if both operands same sign and result different sign */
    bool overflow = ((src ^ result) & (dst ^ result) & msb) != 0;

    cpuSetFlag(cpu, SR_C, carry);
    cpuSetFlag(cpu, SR_X, carry);
    cpuSetFlag(cpu, SR_V, overflow);
    aluSetNZ(cpu, result, sz);

    return result;
}

u32 aluAddX(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    u32 x    = cpuGetFlag(cpu, SR_X) ? 1 : 0;

    src &= mask;
    dst &= mask;
    u32 result = (src + dst + x) & mask;

    bool carry = (u64)src + (u64)dst + x > mask;
    bool overflow = ((src ^ result) & (dst ^ result) & msb) != 0;

    cpuSetFlag(cpu, SR_C, carry);
    cpuSetFlag(cpu, SR_X, carry);
    cpuSetFlag(cpu, SR_V, overflow);
    cpuSetFlag(cpu, SR_N, (result & msb) != 0);
    /* Z is only cleared, never set (for multi-precision) */
    if (result != 0) cpuSetFlag(cpu, SR_Z, false);

    return result;
}

/* ── Subtraction ── */
u32 aluSub(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);

    src &= mask;
    dst &= mask;
    u32 result = (dst - src) & mask;

    /* Borrow: set if src > dst (unsigned) */
    bool borrow = src > dst;

    /* Overflow: src and dst different sign, result and src same sign */
    bool overflow = ((dst ^ src) & (dst ^ result) & msb) != 0;

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_X, borrow);
    cpuSetFlag(cpu, SR_V, overflow);
    aluSetNZ(cpu, result, sz);

    return result;
}

u32 aluSubX(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    u32 x    = cpuGetFlag(cpu, SR_X) ? 1 : 0;

    src &= mask;
    dst &= mask;
    u32 result = (dst - src - x) & mask;

    bool borrow = (u64)src + x > dst;
    bool overflow = ((dst ^ src) & (dst ^ result) & msb) != 0;

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_X, borrow);
    cpuSetFlag(cpu, SR_V, overflow);
    cpuSetFlag(cpu, SR_N, (result & msb) != 0);
    if (result != 0) cpuSetFlag(cpu, SR_Z, false);

    return result;
}

u32 aluNeg(Cpu *cpu, u32 val, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);

    val &= mask;
    u32 result = (0 - val) & mask;

    bool borrow = val != 0;
    bool overflow = (val & result & msb) != 0;

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_X, borrow);
    cpuSetFlag(cpu, SR_V, overflow);
    aluSetNZ(cpu, result, sz);

    return result;
}

u32 aluNegX(Cpu *cpu, u32 val, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    u32 x    = cpuGetFlag(cpu, SR_X) ? 1 : 0;

    val &= mask;
    u32 result = (0 - val - x) & mask;

    bool borrow = (val + x) != 0;
    bool overflow = (result & msb) != 0 && (val & msb) != 0;

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_X, borrow);
    cpuSetFlag(cpu, SR_V, overflow);
    cpuSetFlag(cpu, SR_N, (result & msb) != 0);
    if (result != 0) cpuSetFlag(cpu, SR_Z, false);

    return result;
}

/* ── Compare (subtract without storing result) ── */
void aluCmp(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);

    src &= mask;
    dst &= mask;
    u32 result = (dst - src) & mask;

    bool borrow = src > dst;
    bool overflow = ((dst ^ src) & (dst ^ result) & msb) != 0;

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_V, overflow);
    aluSetNZ(cpu, result, sz);
    /* X is not affected by CMP */
}

/* ── Logical operations ── */
u32 aluAnd(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 result = (src & dst) & sizeMask(sz);
    aluSetLogicFlags(cpu, result, sz);
    return result;
}

u32 aluOr(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 result = (src | dst) & sizeMask(sz);
    aluSetLogicFlags(cpu, result, sz);
    return result;
}

u32 aluEor(Cpu *cpu, u32 src, u32 dst, OperationSize sz) {
    u32 result = (src ^ dst) & sizeMask(sz);
    aluSetLogicFlags(cpu, result, sz);
    return result;
}

u32 aluNot(Cpu *cpu, u32 val, OperationSize sz) {
    u32 result = (~val) & sizeMask(sz);
    aluSetLogicFlags(cpu, result, sz);
    return result;
}

/* ── Shifts and rotates ── */

u32 aluAsl(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    val &= mask;

    bool carry = false;
    bool overflow = false;

    for (int i = 0; i < count; i++) {
        carry = (val & msb) != 0;
        u32 prev = val;
        val = (val << 1) & mask;
        /* V is set if the MSB changes at any point during the shift */
        if ((prev ^ val) & msb) overflow = true;
    }

    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    cpuSetFlag(cpu, SR_X, count > 0 ? carry : cpuGetFlag(cpu, SR_X));
    cpuSetFlag(cpu, SR_V, overflow);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluAsr(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    int bits = (int)sizeInBits(sz);
    val &= mask;

    bool carry = false;
    bool sign  = (val & msb) != 0;

    for (int i = 0; i < count; i++) {
        carry = (val & 1) != 0;
        val = (val >> 1) | (sign ? msb : 0);
        val &= mask;
    }

    (void)bits;
    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    cpuSetFlag(cpu, SR_X, count > 0 ? carry : cpuGetFlag(cpu, SR_X));
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluLsl(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    val &= mask;

    bool carry = false;

    for (int i = 0; i < count; i++) {
        carry = (val & msb) != 0;
        val = (val << 1) & mask;
    }

    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    cpuSetFlag(cpu, SR_X, count > 0 ? carry : cpuGetFlag(cpu, SR_X));
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluLsr(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    val &= mask;

    bool carry = false;

    for (int i = 0; i < count; i++) {
        carry = (val & 1) != 0;
        val = (val >> 1) & mask;
    }

    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    cpuSetFlag(cpu, SR_X, count > 0 ? carry : cpuGetFlag(cpu, SR_X));
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluRol(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    int bits = (int)sizeInBits(sz);
    val &= mask;

    bool carry = false;

    for (int i = 0; i < count; i++) {
        carry = (val & msb) != 0;
        val = ((val << 1) | (carry ? 1 : 0)) & mask;
    }

    (void)bits;
    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    /* X is not affected by ROL */
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluRor(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    val &= mask;

    bool carry = false;

    for (int i = 0; i < count; i++) {
        carry = (val & 1) != 0;
        val = ((val >> 1) | (carry ? msb : 0)) & mask;
    }

    cpuSetFlag(cpu, SR_C, count > 0 ? carry : false);
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluRoxl(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    val &= mask;

    bool x = cpuGetFlag(cpu, SR_X);

    for (int i = 0; i < count; i++) {
        bool msbit = (val & msb) != 0;
        val = ((val << 1) | (x ? 1 : 0)) & mask;
        x = msbit;
    }

    cpuSetFlag(cpu, SR_C, x);
    cpuSetFlag(cpu, SR_X, x);
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

u32 aluRoxr(Cpu *cpu, u32 val, int count, OperationSize sz) {
    u32 mask = sizeMask(sz);
    u32 msb  = sizeMsb(sz);
    val &= mask;

    bool x = cpuGetFlag(cpu, SR_X);

    for (int i = 0; i < count; i++) {
        bool lsbit = (val & 1) != 0;
        val = ((val >> 1) | (x ? msb : 0)) & mask;
        x = lsbit;
    }

    cpuSetFlag(cpu, SR_C, x);
    cpuSetFlag(cpu, SR_X, x);
    cpuSetFlag(cpu, SR_V, false);
    aluSetNZ(cpu, val, sz);

    return val;
}

/* ── Multiplication ── */

u32 aluMulu(Cpu *cpu, u16 src, u16 dst) {
    u32 result = (u32)src * (u32)dst;
    aluSetNZ(cpu, result, SIZE_LONG);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
    return result;
}

u32 aluMuls(Cpu *cpu, s16 src, s16 dst) {
    s32 result = (s32)src * (s32)dst;
    u32 uresult = (u32)result;
    aluSetNZ(cpu, uresult, SIZE_LONG);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
    return uresult;
}

/* ── Division (returns false on divide-by-zero) ── */

bool aluDivu(Cpu *cpu, u32 dividend, u16 divisor, u32 *result) {
    if (divisor == 0) return false;

    u32 quotient  = dividend / divisor;
    u32 remainder = dividend % divisor;

    /* Overflow if quotient > 16 bits */
    if (quotient > 0xFFFF) {
        cpuSetFlag(cpu, SR_V, true);
        /* N, Z, C undefined on overflow */
        *result = dividend; /* unchanged */
        return true;
    }

    *result = (remainder << 16) | (quotient & 0xFFFF);

    aluSetNZ(cpu, quotient, SIZE_WORD);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
    return true;
}

bool aluDivs(Cpu *cpu, s32 dividend, s16 divisor, u32 *result) {
    if (divisor == 0) return false;

    s32 quotient  = dividend / divisor;
    s32 remainder = dividend % divisor;

    /* Overflow if quotient doesn't fit in signed 16 bits */
    if (quotient > 32767 || quotient < -32768) {
        cpuSetFlag(cpu, SR_V, true);
        *result = (u32)dividend;
        return true;
    }

    *result = ((u32)(u16)remainder << 16) | ((u32)(u16)quotient);

    aluSetNZ(cpu, (u32)(u16)quotient, SIZE_WORD);
    cpuSetFlag(cpu, SR_V, false);
    cpuSetFlag(cpu, SR_C, false);
    return true;
}

/* ── BCD operations ── */

u8 aluAbcd(Cpu *cpu, u8 src, u8 dst) {
    u32 x = cpuGetFlag(cpu, SR_X) ? 1 : 0;

    u32 lowNibble  = (dst & 0x0F) + (src & 0x0F) + x;
    u32 highNibble = (dst & 0xF0) + (src & 0xF0);

    if (lowNibble > 0x09) {
        lowNibble += 0x06;
    }
    u32 result = highNibble + lowNibble;

    bool carry = false;
    if (result > 0x99) {
        result += 0x60;
        carry = true;
    }

    cpuSetFlag(cpu, SR_C, carry);
    cpuSetFlag(cpu, SR_X, carry);
    if ((u8)result != 0) cpuSetFlag(cpu, SR_Z, false);
    /* N and V are undefined for ABCD */

    return (u8)result;
}

u8 aluSbcd(Cpu *cpu, u8 src, u8 dst) {
    u32 x = cpuGetFlag(cpu, SR_X) ? 1 : 0;

    s32 lowNibble  = (dst & 0x0F) - (src & 0x0F) - (s32)x;
    s32 highNibble = (dst & 0xF0) - (src & 0xF0);

    if (lowNibble < 0) {
        lowNibble += 0x0A;
        highNibble -= 0x10;
    }
    s32 result = highNibble + lowNibble;

    bool borrow = false;
    if (result < 0) {
        result += 0xA0;
        borrow = true;
    }

    cpuSetFlag(cpu, SR_C, borrow);
    cpuSetFlag(cpu, SR_X, borrow);
    if ((u8)result != 0) cpuSetFlag(cpu, SR_Z, false);

    return (u8)result;
}

u8 aluNbcd(Cpu *cpu, u8 val) {
    return aluSbcd(cpu, val, 0);
}
