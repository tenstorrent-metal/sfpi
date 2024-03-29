/*
 * SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include "sfpu.h"
#include "sfpi_fp16.h"
#include "sfpu_bool.h"

using namespace sfpu;
using namespace sfpi;
using namespace std;

namespace sfpu {

SFPUDReg sfpu_dreg;
SFPUCC sfpu_cc;
__rvtt_vec_t sfpu_lreg[SFPU_LREGS];

};

#define sfpu_assert(x) if (!(x)) throw


static unsigned int cmpx_to_setcc_mod1_map[] = {
  0,
  SFPSETCC_MOD1_LREG_LT0,
  SFPSETCC_MOD1_LREG_EQ0,
  SFPSETCC_MOD1_LREG_GTE0,
  SFPSETCC_MOD1_LREG_NE0,
};

///////////////////////////////////////////////////////////////////////////////
SFPUDReg::SFPUDReg() : addr_offset(0)
{ 
    for (int j = 0; j < SFPU_DREG_SIZE; j++) {
        for (int i = 0; i < SFPU_WIDTH; i++) {
            regs[j][i] = 0xDEAD;
        }
    }
}

void SFPUDReg::load(unsigned int out[SFPU_WIDTH], const int addr) const
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            out[i] = regs[addr][i];
        }
    }
}

void SFPUDReg::store_int(const unsigned int data[SFPU_WIDTH], const int addr)
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            regs[addr + addr_offset][i] = data[i];
        }
    }
}

void SFPUDReg::store_float(const unsigned int data[SFPU_WIDTH], const int addr)
{
    // XXXXXFIXME handle rebias on format A
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            regs[addr + addr_offset][i] = data[i];
        }
    }
}

void SFPUDReg::store_float(const float data[SFPU_WIDTH], const int addr)
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            regs[addr + addr_offset][i] = float_to_int(data[i]);
        }
    }
}

void SFPUDReg::store_float(const float data, const int row, const int col)
{
    if (sfpu_cc.enabled(col)) {
        regs[row + addr_offset][col] = float_to_int(data);
    }
}

///////////////////////////////////////////////////////////////////////////////
SFPUCC::SFPUCC() : stack_ptr(0), deferred_result_pending(false)
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable[i] = false;
        result[i] = true;
        for (int j = 0; j < SFPU_CC_DEPTH; j++) {
            enable_stack[j][i] = false;
            result_stack[j][i] = false;
        }
    }
}

void SFPUCC::set_enable(const bool value)
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable[i] = value;
    }
}

void SFPUCC::comp_enable()
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable[i] = !enable[i];
    }
}

void SFPUCC::set_result(const bool use_enable, const bool value)
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        result[i] = value && (!use_enable || enable[i]);
    }
}

void SFPUCC::deferred_init()
{
    deferred_result_pending = true;
    for (int i = 0; i < SFPU_WIDTH; i++) {
        deferred_result[i] = result[i];
    }
}

void SFPUCC::deferred_commit()
{
    if (deferred_result_pending) {
        for (int i = 0; i < SFPU_WIDTH; i++) {
            result[i] = deferred_result[i];
        }
        deferred_result_pending = false;
    }
}


void SFPUCC::push()
{
    if (stack_ptr == SFPU_CC_DEPTH) {
        fprintf(stderr, "CC stack overflow on push, aborting");
        exit(-1);
    }
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable_stack[stack_ptr][i] = enable[i];
        result_stack[stack_ptr][i] = result[i];
    }

    stack_ptr++;
}

void SFPUCC::replace()
{
    int prior = stack_ptr - 1;

    if (prior < 0) {
        fprintf(stderr, "CC stack replace on empty stack\n");
        throw;
    }
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable_stack[prior][i] = enable[i];
        result_stack[prior][i] = result[i];
    }
}

void SFPUCC::pop()
{
    if (stack_ptr == 0) {
        fprintf(stderr, "CC stack underflow on pop, aborting");
        exit(-1);
    }
    stack_ptr--;
    for (int i = 0; i < SFPU_WIDTH; i++) {
        enable[i] = enable_stack[stack_ptr][i];
        result[i] = result_stack[stack_ptr][i];
    }
}

void SFPUCC::comp()
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (stack_ptr == 0 || (enable_stack[stack_ptr - 1][i] && result_stack[stack_ptr - 1][i])) {
            set_result(true, i, !enabled(i));
        } else {
            set_result(false, i, false);
        }
    }
}

void SFPUCC::deferred_comp()
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (stack_ptr == 0 || (enable_stack[stack_ptr - 1][i] && result_stack[stack_ptr - 1][i])) {
            deferred_set_result(true, i, !enabled(i));
        } else {
            deferred_set_result(false, i, false);
        }
    }
}

void SFPUCC::dump()
{
    char enabled_string[2] = { 'd', 'E' };
    if (deferred_result_pending) {
        printf("cc(d): ");
    } else {
        printf("cc: ");
    }

    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.deferred_result_pending) {
            printf("[%d]%c%c  ", i, enabled_string[!sfpu_cc.enable[i]], enabled_string[sfpu_cc.deferred_result[i]]);
        } else {
            printf("[%d]%c%c  ", i, enabled_string[!sfpu_cc.enable[i]], enabled_string[sfpu_cc.result[i]]);
        }
    }
    printf("\n");
}

void SFPUCC::dump(int i)
{
    char enabled_string[2] = { 'd', 'E' };
    if (sfpu_cc.deferred_result_pending) {
        printf("[%d]d%c%c  ", i, enabled_string[!sfpu_cc.enable[i]], enabled_string[sfpu_cc.deferred_result[i]]);
    } else {
        printf("[%d]%c%c  ", i, enabled_string[!sfpu_cc.enable[i]], enabled_string[sfpu_cc.result[i]]);
    }
}

///////////////////////////////////////////////////////////////////////////////
void SFPUConditional::emit_boolean_tree(int w, bool negate)
{
    const SFPUConditional& node = sfpu_conditionals[w];
    bool negate_node = node.negated;
    bool restore = false;
    __rvtt_vec_t saved_enables = sfpu_rvtt_sfploadi(SFPLOADI_MOD0_SHORT, 1);

    if (node.get_boolean_type(negate) == SFPUConditional::opType::opOr) {
        negate_node = !negate_node;
        negate = !negate;
    }

    // Emit LHS
    emit_conditional(node.op_a, negate);

    // Emit RHS
    if (sfpu_conditionals[node.op_b].is_boolean()) {
        sfpu_rvtt_sfppushc(SFPPUSHC_MOD1_PUSH);
        emit_conditional(node.op_b, negate);
        restore = true;
    } else {
        if (sfpu_conditionals[node.op_b].issues_compc(negate)) {
            restore = true;
            sfpu_rvtt_sfppushc(SFPPUSHC_MOD1_PUSH);
        }
        emit_conditional(node.op_b, negate);
    }

    if (restore) {
        saved_enables = sfpu_rvtt_sfploadi(SFPLOADI_MOD0_SHORT, 0);
        sfpu_rvtt_sfppopc();
        sfpu_rvtt_sfpsetcc_v(saved_enables, SFPSETCC_MOD1_LREG_EQ0);
    }

    if (negate_node) {
        sfpu_rvtt_sfpcompc();
    }
}

///////////////////////////////////////////////////////////////////////////////
constexpr __rvtt_vec_t::__rvtt_vec_t(ConstructType) : values()
{
    // Default constructor is called to initialize Tile-ID for CRegs
    for (int i = 0; i < SFPU_WIDTH; i++) {
        // TileId counts by 2 on wormhole!
        values[i] = i * 2;
    }
}

constexpr __rvtt_vec_t::__rvtt_vec_t(const unsigned int v) : values()
{
    for (int i = 0; i < SFPU_WIDTH; i++) {
        values[i] = v;
    }
}

void __rvtt_vec_t::dump(int start, int stop) const
{
    char enabled_string[2] = { 'd', 'E' };
    for (int i = start; i < stop; i++) {
        printf("reg[%d]: (%c) 0x%x %f\n", i, enabled_string[sfpu_cc.enabled(i)], get_uint(i), get_float(i));
    }
}

void __rvtt_vec_t::dump(int i) const
{
    char enabled_string[2] = { 'd', 'E' };
    printf("reg[%d]: (%c) 0x%x %f\n", i, enabled_string[sfpu_cc.enabled(i)], get_uint(i), get_float(i));
}

///////////////////////////////////////////////////////////////////////////////
class CRegInternal {
 private:
    static __rvtt_vec_t cregs[16];

 public:
    void set(const __rvtt_vec_t& in, int n) { cregs[n] = in; }
    const __rvtt_vec_t& operator[](const int i) const { return cregs[i]; }
};

///////////////////////////////////////////////////////////////////////////////
static CRegInternal kCRegInternal;

__rvtt_vec_t CRegInternal::cregs[16] =
{
    0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF,
    0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF,
    0x3F56594B, 0x00000000, 0x3F800000, 0xBF800000,
    0x00000000, 0x00000000, 0x00000000, __rvtt_vec_t::ConstructType::TileId
};

///////////////////////////////////////////////////////////////////////////////
__rvtt_vec_t sfpu_rvtt_sfpload(unsigned int mod0, unsigned int mode, unsigned int addr)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();

    // XXXXXFIXME handle rebias on format A
    sfpu_dreg.load(tmp.get_data_write(), addr);

    return tmp;
}

///////////////////////////////////////////////////////////////////////////////
__rvtt_vec_t sfpu_rvtt_sfpassignlreg(unsigned int lr)
{
    return (lr < 4) ? sfpu_lreg[lr] : kCRegInternal[lr];
}

void sfpu_rvtt_sfpincrwc(int, int, int b, int)
{
    sfpu_dreg.add_offset(b);
}

void sfpu_rvtt_sfpstore(const __rvtt_vec_t& v, unsigned int mod0, unsigned int addr)
{
    sfpu_cc.deferred_commit();
    if (mod0 & SFPSTORE_MOD0_FMT_INT32_TO_SM) {
        sfpu_dreg.store_int(v.get_data_read(), addr);
    } else {
        sfpu_dreg.store_float(v.get_data_read(), addr);
    }
}

__rvtt_vec_t sfpu_rvtt_sfploadi(unsigned int mod0, unsigned int value)
{
    unsigned int converted;
    switch (mod0) {

    case SFPLOADI_MOD0_FLOATB:
        converted = value << 16;
        break;

    case SFPLOADI_MOD0_FLOATA:
        {
            converted =
                ((value & FP16A_SGN_MASK) << 16) |
                (((value & FP16A_EXP_MASK) - ((FP16A_EXP_BIAS - FP32_EXP_BIAS) << FP16A_EXP_SHIFT)) << (FP32_EXP_SHIFT - FP16A_EXP_SHIFT)) |
                ((value & FP16A_MAN_MASK) << 13);
        }
        break;

    case SFPLOADI_MOD0_USHORT:
        // unsigned int
        converted = value;
        break;

    case SFPLOADI_MOD0_SHORT:
        // signed int
        converted = value;
        if (value & BIT_15_MASK) {
            converted |= 0xFFFF0000;
        }
        break;

    case SFPXLOADI_MOD0_INT32:
    case SFPXLOADI_MOD0_UINT32:
        converted = value;
        break;

    case SFPXLOADI_MOD0_FLOAT:
        converted = value;
        break;

    case SFPLOADI_MOD0_UPPER:
    case SFPLOADI_MOD0_LOWER:
    default:
        fprintf(stderr, "Illegal mod0 in sfploadi: %d\n", mod0);
        throw;
    }

    sfpu_cc.deferred_commit();
    __rvtt_vec_t tmp;
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_uint(i, converted);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpmov(const __rvtt_vec_t& v, unsigned int mod1)
{
    unsigned int mask = 0;

    if (mod1 == SFPMOV_MOD1_COMPSIGN) {
        mask = BIT_31_MASK;
    }

    sfpu_cc.deferred_commit();
    __rvtt_vec_t t;
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            t.set_uint(i, v.get_uint(i) ^ mask);
        }
    }

    return t;
}

__rvtt_vec_t sfpu_rvtt_sfpadd(const __rvtt_vec_t& a, const __rvtt_vec_t& b, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_float(i, a.get_float(i) + b.get_float(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpmul(const __rvtt_vec_t& a, const __rvtt_vec_t& b, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_float(i, a.get_float(i) * b.get_float(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpmad(const __rvtt_vec_t& a, const __rvtt_vec_t& b, const __rvtt_vec_t& c, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_float(i, a.get_float(i) * b.get_float(i) + c.get_float(i));
        }
    }

    return tmp;
}

void sfpu_rvtt_sfpnop()
{
}

void sfpu_rvtt_sfpillegal()
{
    fprintf(stderr, "Too many SFP conditionals in an if, aborting\n");
    exit(-1);
}

void sfpu_rvtt_sfpencc(unsigned int imm12, unsigned int mod1)
{
    sfpu_cc.deferred_commit();
    switch (mod1) {
    case SFPENCC_MOD1_EU_R1:
        sfpu_cc.set_result(false, true);
        break;
    case SFPENCC_MOD1_EC_R1:
        sfpu_cc.comp_enable();
        sfpu_cc.set_result(false, true);
        break;
    case SFPENCC_MOD1_EI_R1:
        sfpu_cc.set_enable((imm12 & 1) == 1);
        sfpu_cc.set_result(false, true);
        break;
    case SFPENCC_MOD1_EU_RI:
        sfpu_cc.set_result(false, (imm12 & 2) == 2);
        break;
    case SFPENCC_MOD1_EC_RI:
        sfpu_cc.comp_enable();
        sfpu_cc.set_result(false, (imm12 & 2) == 2);
        break;
    case SFPENCC_MOD1_EI_RI:
        sfpu_cc.set_enable((imm12 & 1) == 1);
        sfpu_cc.set_result(false, (imm12 & 2) == 2);
        break;
    default:
        fprintf(stderr, "Illegal mod1 in sfpencc: %d\n", mod1);
        throw;
    };
}

void sfpu_rvtt_sfpcompc()
{
    sfpu_cc.deferred_commit();
    sfpu_cc.comp();
}

void sfpu_rvtt_sfppushc(unsigned int mod1)
{
    sfpu_cc.deferred_commit();
    if (mod1 == SFPPUSHC_MOD1_PUSH) {
        sfpu_cc.push();
    } else if (mod1 == SFPPUSHC_MOD1_REPLACE) {
        sfpu_cc.replace();
    } else {
        fprintf(stderr, "Illegal mod1 in sfppushc: %d\n", mod1);
        throw;
    }
}

void sfpu_rvtt_sfppopc()
{
    sfpu_cc.deferred_commit();
    sfpu_cc.pop();
}

void sfpu_rvtt_sfpsetcc_i(unsigned int imm, unsigned int mod1)
{
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (mod1 == SFPEXEXP_MOD1_NODEBIAS) {
            sfpu_cc.and_result(i, (imm & 1) != 0);
        } else if (mod1 == SFPEXEXP_MOD1_SET_CC_COMP_EXP) {
            sfpu_cc.and_result(i, !sfpu_cc.enabled(i));
        }
    }
}

void sfpu_rvtt_sfpsetcc_v(const __rvtt_vec_t& v, unsigned int mod1)
{
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        switch (mod1) {
        case SFPSETCC_MOD1_LREG_LT0:
            sfpu_cc.and_result(i, (v.get_uint(i) & BIT_31_MASK) != 0);
            break;
        case SFPSETCC_MOD1_LREG_NE0:
            sfpu_cc.and_result(i, (v.get_uint(i) & BITS_0_TO_31_MASK) != 0);
            break;
        case SFPSETCC_MOD1_LREG_GTE0:
            sfpu_cc.and_result(i, (v.get_uint(i) & BIT_31_MASK) == 0);
            break;
        case SFPSETCC_MOD1_LREG_EQ0:
            sfpu_cc.and_result(i, (v.get_uint(i) & BITS_0_TO_31_MASK) == 0);
            break;
        case SFPSETCC_MOD1_COMP:
            sfpu_cc.and_result(i, !sfpu_cc.enabled(i));
            break;
        default:
            fprintf(stderr, "Illegal mod1 in sfpsetcc: %d\n", mod1);
            throw;
        }
    }
}

void sfpu_rvtt_sfpxscmp(const __rvtt_vec_t& a, unsigned int b, unsigned int mod1)
{
    unsigned int cmp = mod1 & SFPXCMP_MOD1_CC_MASK;
    
    if (cmp == SFPXCMP_MOD1_CC_LTE || cmp == SFPXCMP_MOD1_CC_GT) {
        unsigned int new_mod = mod1 & ~SFPXCMP_MOD1_CC_MASK;
        // Inefficient
        sfpu_rvtt_sfpxscmp(a, b, new_mod | SFPXCMP_MOD1_CC_GTE);
        sfpu_rvtt_sfpxscmp(a, b, new_mod | SFPXCMP_MOD1_CC_NE);
        if (cmp == SFPXCMP_MOD1_CC_LTE) {
            sfpu_rvtt_sfpcompc();
        }
        return;
    }

    if (b != 0) {
        __rvtt_vec_t tmp;
        int fmt = mod1 & SFPXSCMP_MOD1_FMT_MASK;
        int loadi_mod = (fmt == SFPXSCMP_MOD1_FMT_A) ? SFPLOADI_MOD0_FLOATA :
            ((fmt == SFPXSCMP_MOD1_FMT_B) ? SFPLOADI_MOD0_FLOATB : SFPXLOADI_MOD0_FLOAT);
        __rvtt_vec_t op_b = __builtin_rvtt_sfpxloadi(loadi_mod, b);
        __rvtt_vec_t neg_op_b = __builtin_rvtt_sfpmov(op_b, SFPMOV_MOD1_COMPSIGN);
        tmp = __builtin_rvtt_sfpmad(neg_op_b, __builtin_rvtt_sfpassignlreg(CREG_IDX_1), a, 0);
        __builtin_rvtt_sfpsetcc_v(tmp, cmpx_to_setcc_mod1_map[cmp]);
    } else {
        __builtin_rvtt_sfpsetcc_v(a, cmpx_to_setcc_mod1_map[cmp]);
    }
}

void sfpu_rvtt_sfpxvcmp(const __rvtt_vec_t& a, const __rvtt_vec_t& b, unsigned int mod1)
{
    unsigned int cmp = mod1 & SFPXCMP_MOD1_CC_MASK;

    __rvtt_vec_t neg = __builtin_rvtt_sfpmov(b, SFPMOV_MOD1_COMPSIGN);
    __rvtt_vec_t tmp = __builtin_rvtt_sfpmad(neg,
                                             __builtin_rvtt_sfpassignlreg(CREG_IDX_1),
                                             a,
                                             0);
    if (cmp == SFPXCMP_MOD1_CC_LTE || cmp == SFPXCMP_MOD1_CC_GT) {
        __builtin_rvtt_sfpsetcc_v(tmp, SFPSETCC_MOD1_LREG_GTE0);
        __builtin_rvtt_sfpsetcc_v(tmp, SFPSETCC_MOD1_LREG_NE0);
        if (cmp == SFPXCMP_MOD1_CC_LTE) {
            sfpu_rvtt_sfpcompc();
        }
    } else {
        __builtin_rvtt_sfpsetcc_v(tmp, cmpx_to_setcc_mod1_map[mod1 & SFPXCMP_MOD1_CC_MASK]);
    }
}

__rvtt_vec_t sfpu_rvtt_sfpexexp(const __rvtt_vec_t& v, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    sfpu_cc.deferred_init();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int exp = (v.get_uint(i) >> FP32_EXP_SHIFT) & 0xFF;
            if ((mod1 & SFPEXEXP_MOD1_NODEBIAS) == 0) {
                exp -= FP32_EXP_BIAS;
            }

            tmp.set_uint(i, exp);

            if ((mod1 & (SFPEXEXP_MOD1_SET_CC_COMP_EXP | SFPEXEXP_MOD1_SET_CC_SGN_EXP)) == SFPEXEXP_MOD1_SET_CC_SGN_EXP) {
                sfpu_cc.deferred_and_result(i, exp < 0);
            } else if ((mod1 & 0xA) == 10) {
                sfpu_cc.deferred_and_result(i, exp >= 0);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpexman(const __rvtt_vec_t& v, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            unsigned int man = v.get_uint(i) & FP32_MAN_MASK;
            if (mod1 == SFPEXMAN_MOD1_PAD9) {
                tmp.set_uint(i, man);
            } else if (mod1 == SFPEXMAN_MOD1_PAD8) {
                tmp.set_uint(i, man | (FP32_MAN_MASK + 1));
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetexp_i(unsigned int imm, const __rvtt_vec_t& v)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // Keep sign bit and mantissa
            tmp.set_uint(i, (v.get_uint(i) & FP32_SGN_MAN_MASK) | ((imm & 0xFF) << FP32_EXP_SHIFT));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetexp_v(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // Keep sign bit and mantissa
            tmp.set_uint(i, (src.get_uint(i) & FP32_SGN_MAN_MASK) | ((dst.get_uint(i) & 0xFF) << FP32_EXP_SHIFT));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetman_i(unsigned int imm, const __rvtt_vec_t& v, unsigned int)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // Keep sign bit and exponent
            tmp.set_uint(i, (v.get_uint(i) & FP32_SGN_EXP_MASK) | (imm & FP32_MAN_MASK));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetman_v(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // Keep sign bit and exponent
            tmp.set_uint(i, (src.get_uint(i) & FP32_SGN_EXP_MASK) | (dst.get_uint(i) & FP32_MAN_MASK));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpabs(const __rvtt_vec_t& v, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            if (mod1 == SFPABS_MOD1_INT) {
                // 19 bit int...relies on upper bits being the sign extension
                int32_t n = v.get_uint(i);
                if (n < 0) n = -n;
                tmp.set_uint(i, n);
            } else {
                // FP32, clear sign bit - 31
                tmp.set_uint(i, v.get_uint(i) & BITS_0_TO_31_MASK);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpand(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_uint(i, src.get_uint(i) & dst.get_uint(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpor(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_uint(i, src.get_uint(i) | dst.get_uint(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpxor(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_uint(i, src.get_uint(i) ^ dst.get_uint(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpnot(const __rvtt_vec_t& v)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // XXXXX FIXME  - sets high bits above 18, does it matter? think
            // through whether high bits should always be 0
            // pretty sure this is a bug...
            tmp.set_uint(i, ~v.get_uint(i));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpmuli(const __rvtt_vec_t& v, unsigned short imm, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    float f = fp16b_to_float(imm);
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_float(i, v.get_float(i) * f);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpaddi(const __rvtt_vec_t& v, unsigned short imm, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    float f = fp16b_to_float(imm);
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_float(i, v.get_float(i) + f);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpdivp2(unsigned short imm, const __rvtt_vec_t& v, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    unsigned int exp = (imm & 0xFF) << FP32_EXP_SHIFT;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            unsigned int old = v.get_uint(i);

            unsigned int setexp;
            if (mod1 == SFPSDIVP2_MOD1_ADD) {
                setexp = ((old & FP32_EXP_MASK) + exp) & FP32_EXP_MASK;
            } else {
                setexp = exp;
            }
            tmp.set_uint(i, ((old & (FP32_SGN_MASK | FP32_MAN_MASK)) | setexp));
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfplz(const __rvtt_vec_t& v, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    sfpu_cc.deferred_init();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            unsigned int val = v.get_uint(i);

            if (mod1 & SFPLZ_MOD1_NOSGN_MASK) {
                val &= 0x7FFFFFFF;
            }

            int b;
            for (b = FP32_MAX_BIT; b >= 0; b--) {
                if ((val & (1 << b)) != 0) {
                    break;
                }
            }

            tmp.set_uint(i, FP32_MAX_BIT - b);

            switch (mod1 & ~SFPLZ_MOD1_NOSGN_MASK) {
            case SFPLZ_MOD1_CC_NONE:
                break;
            case SFPLZ_MOD1_CC_NE0:
                sfpu_cc.deferred_and_result(i, val != 0);
                break;
            case SFPLZ_MOD1_CC_COMP:
                sfpu_cc.deferred_comp();
                break;
            case SFPLZ_MOD1_CC_EQ0:
                sfpu_cc.deferred_and_result(i, val == 0);
                break;
            default:
                fprintf(stderr, "Illegal mod1 in sfplz: %d\n", mod1);
                throw;
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpshft_i(const __rvtt_vec_t& dst, int shift)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            if (shift < 0) {
                tmp.set_uint(i, dst.get_uint(i) >> -shift);
            } else {
                tmp.set_uint(i, dst.get_uint(i) << shift);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpshft_v(const __rvtt_vec_t& dst, const __rvtt_vec_t&src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int shift = src.get_uint(i);
            if (shift < 0) {
                tmp.set_uint(i, dst.get_uint(i) >> -shift);
            } else {
                tmp.set_uint(i, dst.get_uint(i) << shift);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpiadd_i(short imm, const __rvtt_vec_t& src, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    // Note: the instruction for iadd uses a 12 bit imm, but the builtin
    // supports 16 bit imms using loadi
    sfpu_cc.deferred_commit();
    sfpu_cc.deferred_init();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int val = src.get_uint(i) + imm;
            tmp.set_uint(i, val);

            if (mod1 & SFPIADD_MOD1_CC_GTE0) {
                sfpu_cc.deferred_and_result(i, val >= 0);
            } else if ((mod1 & SFPIADD_MOD1_CC_NONE) == 0) {
                // LT0
                sfpu_cc.deferred_and_result(i, val < 0);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpxiadd_i(int imm, const __rvtt_vec_t& src, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    // Note: the instruction for iadd uses a 12 bit imm, but the builtin
    // supports 16 bit imms using loadi
    sfpu_cc.deferred_commit();
    sfpu_cc.deferred_init();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int val = src.get_uint(i);
            val += (mod1 & SFPXIADD_MOD1_IS_SUB) ? -imm : imm;
            tmp.set_uint(i, val);

            switch (mod1 & SFPXCMP_MOD1_CC_MASK) {
            case SFPXCMP_MOD1_CC_GTE:
                sfpu_cc.deferred_and_result(i, val >= 0);
                break;
            case SFPXCMP_MOD1_CC_LT:
                sfpu_cc.deferred_and_result(i, val < 0);
                break;
            case SFPXCMP_MOD1_CC_EQ:
                sfpu_cc.deferred_and_result(i, val == 0);
                break;
            case SFPXCMP_MOD1_CC_NE:
                sfpu_cc.deferred_and_result(i, val != 0);
                break;
            case SFPXCMP_MOD1_CC_LTE:
                sfpu_cc.deferred_and_result(i, val <= 0);
                break;
            case SFPXCMP_MOD1_CC_GT:
                sfpu_cc.deferred_and_result(i, val > 0);
                break;
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpiadd_v(const __rvtt_vec_t& dst, const __rvtt_vec_t& src, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    sfpu_cc.deferred_init();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int val;
            if (mod1 & SFPIADD_MOD1_ARG_2SCOMP_LREG_DST) {
                val = src.get_uint(i) - dst.get_uint(i);
            } else {
                // Positive LREG_DST
                val = src.get_uint(i) + dst.get_uint(i);
            }

            tmp.set_uint(i, val);

            if (mod1 & SFPIADD_MOD1_CC_GTE0) {
                sfpu_cc.deferred_and_result(i, val >= 0);
            } else if ((mod1 & SFPIADD_MOD1_CC_NONE) == 0) {
                // LT0
                sfpu_cc.deferred_and_result(i, val < 0);
            }
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpxiadd_v(const __rvtt_vec_t& dst, const __rvtt_vec_t& src, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    unsigned int cmp = mod1 & SFPXCMP_MOD1_CC_MASK;

    bool is_sub = ((mod1 & SFPXIADD_MOD1_IS_SUB) != 0);
    unsigned int mod = is_sub ? SFPIADD_MOD1_ARG_2SCOMP_LREG_DST : SFPIADD_MOD1_ARG_LREG_DST;
    if (cmp == SFPXCMP_MOD1_CC_LT || cmp == SFPXCMP_MOD1_CC_GTE) {
        // Perform op w/ compare
        mod |= (cmp == SFPXCMP_MOD1_CC_LT) ? SFPIADD_MOD1_CC_LT0 : SFPIADD_MOD1_CC_GTE0;
        tmp = sfpu_rvtt_sfpiadd_v(dst, src, mod);
    } else if (cmp == SFPXCMP_MOD1_CC_LTE || cmp == SFPXCMP_MOD1_CC_GT) {
        // Perform op w/o compare, compare w/ IADDI
        tmp = sfpu_rvtt_sfpxiadd_v(dst, src, (mod1 & ~SFPXCMP_MOD1_CC_MASK) | SFPXCMP_MOD1_CC_GTE);
        sfpu_rvtt_sfpsetcc_v(tmp, SFPSETCC_MOD1_LREG_NE0);
        if (cmp == SFPXCMP_MOD1_CC_LTE) {
            sfpu_rvtt_sfpcompc();
        }
    } else {
        // Perform op w/o compare, compare with SETCC
        mod |= SFPIADD_MOD1_CC_NONE;
        tmp = sfpu_rvtt_sfpiadd_v(dst, src, mod);
        if (cmp != 0) {
            sfpu_rvtt_sfpsetcc_v(tmp, cmpx_to_setcc_mod1_map[cmp & SFPXCMP_MOD1_CC_MASK]);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetsgn_i(unsigned short imm, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    unsigned int sign = (static_cast<unsigned int>(imm) & 1) << FP32_SGN_SHIFT;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            tmp.set_uint(i, (src.get_uint(i) & (FP32_EXP_MASK | FP32_MAN_MASK)) | sign);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpsetsgn_v(const __rvtt_vec_t& dst, const __rvtt_vec_t& src)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            unsigned int sign = dst.get_uint(i) & FP32_SGN_MASK;
            tmp.set_uint(i, (src.get_uint(i) & (FP32_EXP_MASK | FP32_MAN_MASK)) | sign);
        }
    }

    return tmp;
}

void sfpu_rvtt_sfpxcondb(int tree, int)
{
    SFPUConditional::emit_conditional(tree, false);
    sfpu_conditionals.resize(0);
}

__rvtt_vec_t sfpu_rvtt_sfpxcondi(int w)
{
    __rvtt_vec_t tmp;

    tmp = sfpu_rvtt_sfploadi(SFPLOADI_MOD0_SHORT, 0);
    __builtin_rvtt_sfppushc();
    sfpu_rvtt_sfpxcondb(w, 0);
    tmp = sfpu_rvtt_sfploadi(SFPLOADI_MOD0_SHORT, 1);
    __builtin_rvtt_sfppopc();

    return tmp;
}

inline float lut_to_fp32(unsigned short v)
{
    union Converter {
        float f;
        uint32_t i;
    } conv;

    if (v == 0xFF) {
        conv.i = 0;
    } else {
        uint32_t sgn = (v & 0x80) >> 7;
        uint32_t exp = 127 - ((v & 0x70) >> 4); // Note: exponent extender is always positive!
        uint32_t man = (v & 0x0F);
        conv.i = (sgn << FP32_SGN_SHIFT) | (exp << FP32_EXP_SHIFT) | (man << (FP32_MAN_WIDTH - 4));
    }

    return conv.f;
}


__rvtt_vec_t sfpu_rvtt_sfplut(const __rvtt_vec_t& l0,
                              const __rvtt_vec_t& l1,
                              const __rvtt_vec_t& l2,
                              const __rvtt_vec_t& dst,
                               unsigned short mod0)
{
    bool retain_sgn = ((mod0 & SFPLUT_MOD0_SGN_RETAIN) != 0);

    __rvtt_vec_t tmp;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            int exp = ((dst.get_uint(i) & FP32_EXP_MASK) >> FP32_EXP_SHIFT) - FP32_EXP_BIAS;

            const __rvtt_vec_t& which = (exp < 0) ? l0 : (exp == 0) ? l1 : l2;
            unsigned int in = which.get_uint(i);
            unsigned short a_in = (in >> 8) & 0xFF;
            unsigned short b_in = in & 0xFF;

            float a = lut_to_fp32(a_in);
            float b = lut_to_fp32(b_in);
            float lr3 = dst.get_float(i);

            float result = a * fabs(lr3) + b;

            if (retain_sgn) {
                union {
                    float fval;
                    uint32_t ival;
                };
                fval = lr3;
                uint32_t sgn = ival & FP32_SGN_MASK;
                fval = result;
                uint32_t rest = ival & ~FP32_SGN_MASK;
                ival = rest | sgn;
                result = fval;
            }

            tmp.set_float(i, result);
        }
    }

    return tmp;
}

float lut2_to_fp32(unsigned short in)
{
    // Handle modified exp, if exp is all 1s, make it all 0s
    if ((in & FP16A_EXP_MASK) == FP16A_EXP_MASK) {
        in &= ~FP16A_EXP_MASK;
    }
    return fp16a_to_float(in);
}

__rvtt_vec_t sfpu_rvtt_sfplutfp32_3r(const __rvtt_vec_t& l0,
                                     const __rvtt_vec_t& l1,
                                     const __rvtt_vec_t& l2,
                                     const __rvtt_vec_t& dst,
                                     unsigned short mod0)
{
    sfpu_assert((mod0 == SFPLUTFP32_MOD0_FP16_3ENTRY_TABLE) ||
                mod0 == (SFPLUTFP32_MOD0_FP16_3ENTRY_TABLE | SFPLUT_MOD0_SGN_RETAIN));

    bool retain_sgn = ((mod0 & SFPLUTFP32_MOD0_SGN_RETAIN) != 0);

    __rvtt_vec_t tmp;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            float val = abs(dst.get_float(i));

            const __rvtt_vec_t& which = (val < 1.0f) ? l0 : (val < 2.0) ? l1 : l2;

            unsigned int in = which.get_uint(i);
            unsigned short a_in = in >> 16;
            unsigned short b_in = in & 0xFFFF;

            float a = lut2_to_fp32(a_in);
            float b = lut2_to_fp32(b_in);
            float lr3 = dst.get_float(i);

            float result = a * fabs(lr3) + b;

            if (retain_sgn) {
                union {
                    float fval;
                    uint32_t ival;
                };
                fval = lr3;
                uint32_t sgn = ival & FP32_SGN_MASK;
                fval = result;
                uint32_t rest = ival & ~FP32_SGN_MASK;
                ival = rest | sgn;
                result = fval;
            }

            tmp.set_float(i, result);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfplutfp32_6r(const __rvtt_vec_t& l0,
                                     const __rvtt_vec_t& l1,
                                     const __rvtt_vec_t& l2,
                                     const __rvtt_vec_t& l4,
                                     const __rvtt_vec_t& l5,
                                     const __rvtt_vec_t& l6,
                                     const __rvtt_vec_t& dst,
                                     unsigned short mod0)
{
    bool retain_sgn = ((mod0 & SFPLUTFP32_MOD0_SGN_RETAIN) != 0);
    mod0 = mod0 & ~SFPLUTFP32_MOD0_SGN_RETAIN;

    __rvtt_vec_t tmp;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            float val = abs(dst.get_float(i));
            float a, b;

            if (mod0 == SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1) {
                if (val < 0.5f) {
                    a = lut2_to_fp32(l0.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l4.get_uint(i) & 0xFFFF);
                } else if (val < 1.0f) {
                    a = lut2_to_fp32(l0.get_uint(i) >> 16);
                    b = lut2_to_fp32(l4.get_uint(i) >> 16);
                } else if (val < 1.5f) {
                    a = lut2_to_fp32(l1.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l5.get_uint(i) & 0xFFFF);
                } else if (val < 2.0f) {
                    a = lut2_to_fp32(l1.get_uint(i) >> 16);
                    b = lut2_to_fp32(l5.get_uint(i) >> 16);
                } else if (val < 3.0f) {
                    a = lut2_to_fp32(l2.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l6.get_uint(i) & 0xFFFF);
                } else {
                    a = lut2_to_fp32(l2.get_uint(i) >> 16);
                    b = lut2_to_fp32(l6.get_uint(i) >> 16);
                }
            } else if (mod0 == SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE2) {
                if (val < 0.5f) {
                    a = lut2_to_fp32(l0.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l4.get_uint(i) & 0xFFFF);
                } else if (val < 1.0f) {
                    a = lut2_to_fp32(l0.get_uint(i) >> 16);
                    b = lut2_to_fp32(l4.get_uint(i) >> 16);
                } else if (val < 1.5f) {
                    a = lut2_to_fp32(l1.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l5.get_uint(i) & 0xFFFF);
                } else if (val < 2.0f) {
                    a = lut2_to_fp32(l1.get_uint(i) >> 16);
                    b = lut2_to_fp32(l5.get_uint(i) >> 16);
                } else if (val < 4.0f) {
                    a = lut2_to_fp32(l2.get_uint(i) & 0xFFFF);
                    b = lut2_to_fp32(l6.get_uint(i) & 0xFFFF);
                } else {
                    a = lut2_to_fp32(l2.get_uint(i) >> 16);
                    b = lut2_to_fp32(l6.get_uint(i) >> 16);
                }
            } else { // FP32 table
                sfpu_assert(mod0 == SFPLUTFP32_MOD0_FP32_3ENTRY_TABLE);

                if (val < 1.0f) {
                    a = l0.get_float(i);
                    b = l4.get_float(i);
                } else if (val < 2.0f) {
                    a = l1.get_float(i);
                    b = l5.get_float(i);
                } else {
                    a = l2.get_float(i);
                    b = l6.get_float(i);
                }
            }

            float lr3 = dst.get_float(i);
            float result = a * fabs(lr3) + b;

            if (retain_sgn) {
                union {
                    float fval;
                    uint32_t ival;
                };
                fval = lr3;
                uint32_t sgn = ival & FP32_SGN_MASK;
                fval = result;
                uint32_t rest = ival & ~FP32_SGN_MASK;
                ival = rest | sgn;
                result = fval;
            }

            tmp.set_float(i, result);
        }
    }

    return tmp;
}

// This won't match the hw since it is using rand(), but matches the spirit
static float stoch_rnd(float x)
{
    float frac = abs(x - trunc(x));

    float random = (float)rand() / RAND_MAX;

    float adjust = (random < frac) ? 1 : 0;

    if (x < 0) adjust *= -1.0f;

    return trunc(x) + adjust;
}

__rvtt_vec_t sfpu_rvtt_sfpcast(const __rvtt_vec_t& src, unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            unsigned int val = tmp.get_uint(i);
            float fval = static_cast<float>(val);
            float result;

            if (mod1 == SFPCAST_MOD1_RND_STOCH) {
                result = stoch_rnd(fval);
            } else {
                // XXXX not implemented!!!
                // not sure if anyone will ever use the emulator for actual
                // kernels, getting all this to work for the compiler test does
                // not have a worthile ROI
                result = NAN;
            }

            tmp.set_float(i, result);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpstochrnd_i(const unsigned int mode,
                                     const unsigned int imm8, const __rvtt_vec_t& srcc,
                                     unsigned int mod1)
{
    __rvtt_vec_t tmp;

    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {
            // XXXX not implemented!!!
            // not sure if anyone will ever use the emulator for actual
            // kernels, getting all this to work for the compiler test does
            // not have a worthile ROI
            float result = -0.0;

            tmp.set_uint(i, result);
        }
    }

    return tmp;
}

__rvtt_vec_t sfpu_rvtt_sfpstochrnd_v(const unsigned int mode,
                                     const __rvtt_vec_t& srcb, const __rvtt_vec_t& srcc,
                                     unsigned int mod1)
{
    __rvtt_vec_t tmp;
    sfpu_cc.deferred_commit();
    for (int i = 0; i < SFPU_WIDTH; i++) {
        if (sfpu_cc.enabled(i)) {

            // XXXX not implemented!!!
            // not sure if anyone will ever use the emulator for actual
            // kernels, getting all this to work for the compiler test does
            // not have a worthile ROI
            float result = -0.0;

            tmp.set_uint(i, result);
        }
    }

    return tmp;
}

void sfpu_rvtt_sfptransp(__rvtt_vec_t& l0, __rvtt_vec_t& l1,
                         __rvtt_vec_t& l2, __rvtt_vec_t& l3)
{
    __rvtt_vec_t* table[] = { &l0, &l1, &l2, &l3};

    sfpu_cc.deferred_commit();
    for (int k = 0; k < SFPU_ROWS; k++) {
        for (int r = k; r < SFPU_ROWS; r++) {
            for (int c = 0; c < SFPU_COLS; c++) {
                unsigned int tmp;
                tmp = table[k]->get_uint(r * SFPU_COLS + c);

                table[k]->set_uint(r * SFPU_COLS + c,
                                   table[r]->get_uint(k * SFPU_COLS + c));
                table[r]->set_uint(k * SFPU_COLS + c, tmp);
            }
        }
    }
}

// The emulator could just call shft directly, there may be a future reason to
// split this out...
__rvtt_vec_t sfpu_rvtt_sfpshft2_i(const __rvtt_vec_t& dst, int shift)
{
    return sfpu_rvtt_sfpshft_i(dst, shift);
}

// Like above
__rvtt_vec_t sfpu_rvtt_sfpshft2_v(const __rvtt_vec_t& dst, const __rvtt_vec_t&src)
{
    return sfpu_rvtt_sfpshft_v(dst, src);
}

// _g is the "global" version, no explicit destination, l0..l3 all written
void sfpu_rvtt_sfpshft2_g(__rvtt_vec_t& l0, __rvtt_vec_t& l1,
                          __rvtt_vec_t& l2, __rvtt_vec_t& l3,
                          int mod)
{
    sfpu_assert(mod == SFPSHFT2_MOD1_COPY4 ||
                mod == SFPSHFT2_MOD1_SUBVEC_CHAINED_COPY4);

    sfpu_cc.deferred_commit();
    if (mod == SFPSHFT2_MOD1_COPY4) {
        for (int i = 0; i < SFPU_WIDTH; i++) {
            if (sfpu_cc.enabled(i)) {
                l0.set_uint(i, l1.get_uint(i));
                l1.set_uint(i, l2.get_uint(i));
                l2.set_uint(i, l3.get_uint(i));
                l3.set_uint(i, 0);
            }
        }
    } else if (mod == SFPSHFT2_MOD1_SUBVEC_CHAINED_COPY4) {
        for (int r = 0; r < SFPU_ROWS; r++) {
            for (int c = 0; c < SFPU_COLS; c++) {
                int i = r * SFPU_COLS + c;

                if (sfpu_cc.enabled(i)) {
                    l0.set_uint(i, l1.get_uint(i));
                    l1.set_uint(i, l2.get_uint(i));
                    l2.set_uint(i, l3.get_uint(i));
                    if (r == 3) {
                        l3.set_uint(i, 0);
                    } else {
                        int i_next_row = (r + 1) * SFPU_COLS + c;
                        l3.set_uint(i, l0.get_uint(i_next_row));
                    }
                }
            }
        }

    }
}

// _ge is both explicit and global, takes a src and writes l0..l3
void sfpu_rvtt_sfpshft2_ge(const __rvtt_vec_t& src,
                           __rvtt_vec_t& l0, __rvtt_vec_t& l1,
                           __rvtt_vec_t& l2, __rvtt_vec_t& l3)
{
    // subvec_shflror1_and_vec_copy4
    sfpu_cc.deferred_commit();
    for (int r = 0; r < SFPU_ROWS; r++) {
        for (int c = 0; c < SFPU_COLS; c++) {
            int i = r * SFPU_COLS + c;

            if (sfpu_cc.enabled(i)) {
                l0.set_uint(i, l1.get_uint(i));
                l1.set_uint(i, l2.get_uint(i));
                l2.set_uint(i, l3.get_uint(i));
                if (c == 0) {
                    int i0 = r * SFPU_COLS + 7;
                    l3.set_uint(i, src.get_uint(i0));
                } else {
                    l3.set_uint(i, src.get_uint(i - 1));
                }
            }
        }
    }
}

// _e is the explicit (operands) version
__rvtt_vec_t sfpu_rvtt_sfpshft2_e(const __rvtt_vec_t& src,
                                  int mod)
{
    __rvtt_vec_t tmp;

    sfpu_assert(mod == SFPSHFT2_MOD1_SUBVEC_SHFLROR1 ||
                mod == SFPSHFT2_MOD1_SUBVEC_SHFLSHR1);

    sfpu_cc.deferred_commit();
    for (int r = 0; r < SFPU_ROWS; r++) {
        for (int c = 0; c < SFPU_COLS; c++) {
            int i = r * SFPU_COLS + c;

            if (sfpu_cc.enabled(i)) {
                if (c == 0) {
                    if (mod == SFPSHFT2_MOD1_SUBVEC_SHFLSHR1) {
                        tmp.set_uint(i, 0);
                    } else {
                        int i0 = r * SFPU_COLS + 7;
                        tmp.set_uint(i, src.get_uint(i0));
                    }
                } else {
                    tmp.set_uint(i, src.get_uint(i - 1));
                }
            }
        }
    }

    return tmp;
}

void sfpu_rvtt_sfpswap(__rvtt_vec_t& dst, __rvtt_vec_t& src, int mod)
{
    static const int direction[][4] = {
        {0, 0, 0, 0}, // unused
        {0, 0, 0, 0}, // VEC_MIN_MAX
        {0, 0, 1, 1}, // SUBVEC_MIN01_MAX23
        {0, 1, 0, 1}, // SUBVEC_MIN02_MAX13
        {0, 1, 1, 0}, // SUBVEC_MIN03_MAX14
        {0, 1, 1, 1}, // SUBVEC_MIN0_MAX123
        {1, 0, 1, 1}, // SUBVEC_MIN1_MAX023
        {1, 1, 0, 1}, // SUBVEC_MIN2_MAX013
        {1, 1, 1, 0}, // SUBVEC_MIN3_MAX012
    };

    sfpu_cc.deferred_commit();
    for (int r = 0; r < SFPU_ROWS; r++) {
        for (int c = 0; c < SFPU_COLS; c++) {
            int i = r * SFPU_COLS + c;
            
            if (sfpu_cc.enabled(i)) {
                unsigned int srcv = src.get_uint(i);
                unsigned int dstv = dst.get_uint(i);

                // Do a signed magnitude compare
                unsigned int max, min;

                // I'm sure there's a better clever way to do this...
                if ((srcv & 0x80000000) && (dstv & 0x80000000)) {
                    max = std::min(srcv, dstv);
                    min = std::max(srcv, dstv);
                } else {
                    max = std::max(srcv, dstv);
                    min = std::min(srcv, dstv);
                }

                if (mod == SFPSWAP_MOD1_SWAP) {
                    src.set_uint(i, dstv);
                    dst.set_uint(i, srcv);
                } else {
                    if (direction[mod][r] == 0) {
                        src.set_uint(i, max);
                        dst.set_uint(i, min);
                    } else {
                        src.set_uint(i, min);
                        dst.set_uint(i, max);
                    }
                }
            }
        }
    }
}

void sfpu_rvtt_sfpconfig_v(const __rvtt_vec_t& l0, unsigned int config_dest)
{
    if (config_dest >= SFPCONFIG_DEST_LREG11 &&
        config_dest <= SFPCONFIG_DEST_LREG14) {
        kCRegInternal.set(l0, config_dest);
    } else {
        // Most config bits aren't implemented
        fprintf(stderr, "Unimplemented sfpconfig call\n");
        throw;
    }
}
