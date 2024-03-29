/*
 * SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <ckernel_ops.h>
#include <cstdint>
#include "test.h"
#include <limits>
#include <cstdint>

#include "sfpi.h"

#if defined(__GNUC__) && !defined(__clang__)
#define sfpi_test_noinline __attribute__ ((noinline)) 
#else
#define sfpi_test_noinline
#endif

#define FWLOG1(x, y)
#define NOC_NODE_ID 0
#define NOC_CMD_BUF_READ_REG(x, y, z) 0
#define NOC_NODE_ID_MASK 0
#define NOC_ADDR_NODE_ID_BITS 0

typedef uint32_t uint;

enum SfpuType {
    tanh,
    tanh_derivative,
    sigmoid,
    gelu,
    sqrt,
    exponential,
    dropout,
    greater_than_equal_zero,
    not_equal_zero,
    greater_than_zero,
    equal_zero,
    less_than_equal_zero
};

enum p_sfpu {
    LREG0,
    LREG1,
    LREG2,
    LREG3
};

enum p_exp {
    C23_73,
    ADJ_EXP,
    FRAC_BITS,
};

enum p_gpr_math {
    DEST_OP0_BASE,
    DEST_OP1_BASE,
};

#define DEST_REGW_BASE_Base_ADDR32 0


using namespace sfpi;

namespace ckernel
{
namespace sfpu
{
    
sfpi_inline vInt sfpu_is_fp16_zero(const vFloat& v, uint exponent_size_8)
{
    if (exponent_size_8) {
        // fp16b
        return v == 0.0F;
    } else {
        // fp16a
        // if math data format is fp16, SFPU will convert 5 bit exp to 8 bit exp
        // in grayskull, this unconditionally adds bias value to exp (even for zero)
        vInt tmp = 0x3800; // loads {0, 8'd112, 10'b0}
        tmp += reinterpret<vInt>(v);

        return tmp == 0;
    }
}

sfpi_inline vFloat sfpu_exp(vFloat val)
{
    // If exponent is > -1 extract it and replace with -1
    vInt exp = exexp(val);
    v_if (exp >= 0) {
        val = setexp(val, 126);
    }
    v_endif;

    // Run series in Horner form
    vFloat tmp = val * vConst0p8369 + 0.8634F;
    val = val * tmp + vConst1;

    v_if (exp >= 0) {
        val = val * val;
        #pragma GCC unroll 0
        for (int s_iter = 0; s_iter < 7; s_iter++) {
            exp = exp - 1;
            // Narrow predication on each loop
            v_and(exp >= 0);
            val = val * val;
        }
    }
    v_endif;

    return val;
}

template <bool save_reg, int max_iter = 3>
sfpi_inline vFloat sfpu_reciprocal(const vFloat in)
{
    vInt orig_exp;

    if constexpr (max_iter == 1) {
        // If we are only doing one iteration of the MAD loop, then we only need to use one LREG for the MAD instructions because we have our "first guess" in a hard-coded register
        // This allows us to avoid having to load back in the original value later on
        orig_exp = exexp(in);
    }

    // Force sign to 1 (make number negative)
    vFloat val = setsgn(in, 1);

    val = setexp(val, 126); // Set exponent to 126 to make the number in 0.5-1
    // Use 1.44 as first guess at x, ideal value would be 1.33, but we happen to have 1.44 available, so use that to avoid a load
    vFloat two;
    if (!save_reg) {
        two = 2.0f;
    }
    vFloat result = vConst1p4424 * (val * vConst1p4424 + (save_reg ? 2.0f : two));

    for (int s_iter = 0; s_iter < (max_iter-1); s_iter++) {
        result = result * (val * result + (save_reg ? 2.0f : two));
    }

    vInt new_exp = exexp(result);
    if constexpr (max_iter != 1) {
        orig_exp = exexp(dst_reg[0]);
    }

    // "Subtract" exponents, and re-bias.  
    // Execute: -1 - exp, then exp += 127
    new_exp -= orig_exp;
    new_exp += 126;

    v_if (new_exp < 0) {
        // If rebiased exponent is negative, we need to saturate at 0.
        // This means the initial number was too big so reciprocal result should be 0
        result = 0.0F;
        new_exp = 0;
    }
    v_endif;

    // Set newly denormalized exponent to result exponent field
    return setexp(result, new_exp);
}

inline void init_dropout_seed(uint16_t p2)
{
    FWLOG1("calculate_dropout() -- input seed:%x", p2);
    
    uint32_t noc_id_reg = NOC_CMD_BUF_READ_REG(0, 0, NOC_NODE_ID);

    uint16_t my_x = noc_id_reg & NOC_NODE_ID_MASK;
    uint16_t my_y = (noc_id_reg >> NOC_ADDR_NODE_ID_BITS) & NOC_NODE_ID_MASK;

    uint16_t per_tensix_input_seed = p2 ^ (my_x << my_y);

    FWLOG1("calculate_dropout() -- calculated seed:%x", per_tensix_input_seed);
    
    vInt result;
    result = l_reg[LRegs::LReg3];

    vInt tmp = vConstTileId << 13;
    vInt ptis = reinterpret<vInt>(vFloat(per_tensix_input_seed));
    result = ~(tmp & ptis) & (tmp | ptis);

    l_reg[LRegs::LReg3] = result;
}

template <bool APPROXIMATION_MODE>
inline void sfpu_init(SfpuType operation, uint param0 = 0) 
{
    uint imm0;
    uint imm1;
    uint imm2;
    switch (operation) {
    case SfpuType::tanh:
    case SfpuType::tanh_derivative:
        imm0 = 0x1DFF; //0.90625*x
        imm1 = 0x481A; //0.09375*x + 0.8125
        imm2 = 0xFF00; //1
        TTI_SFPLOADI(0, 2, imm0);
        TTI_SFPLOADI(1, 2, imm1);
        TTI_SFPLOADI(2, 2, imm2);
        break;
    case SfpuType::sigmoid:
        imm0 = 0x3DFF;
        imm1 = 0x21D8;
        imm2 = 0xFF10;
        TTI_SFPLOADI(0, 2, imm0);
        TTI_SFPLOADI(1, 2, imm1);
        TTI_SFPLOADI(2, 2, imm2);
        break;
    case SfpuType::gelu:
        imm0 = 0x18FF;
        imm1 = (APPROXIMATION_MODE)? 0x212C : 0x2010;
        imm2 = 0xFF00;
        TTI_SFPLOADI(0, 2, imm0);
        TTI_SFPLOADI(1, 2, imm1);
        TTI_SFPLOADI(2, 2, imm2);
        break;
    case SfpuType::sqrt:
        if (APPROXIMATION_MODE) {
            TTI_SFPLOADI(2, 0, 127 << 7);
        }
        break;
    case SfpuType::exponential:
        if constexpr(APPROXIMATION_MODE) {
            TTI_SFPLOADI(p_sfpu::LREG0, 0, p_exp::C23_73);
            TTI_SFPLOADI(p_sfpu::LREG2, 0, p_exp::ADJ_EXP);
        }
        break;
    case SfpuType::dropout:
        init_dropout_seed(param0);
        break;
    default:
        // Should result in compile time error??
        break;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_inline vFloat calculate_exponential_body(vFloat in)
{
    vFloat out;

    if constexpr (APPROXIMATION_MODE)
    {
        constexpr int FRAC_BITS = 3;
        constexpr uint SP_BIAS = 127 << FRAC_BITS;

        // * by 1/ln2 and add convert to 7.3 FxP format
        vFloat conv = in * vConst1p4424;

        // Clear exp bits
        vInt c23_73 = p_exp::C23_73;
        vInt tmp = reinterpret<vInt>(conv) - c23_73;

        // Add bias
        tmp += SP_BIAS;

        // SHL to move integer bits to exponent
        out = reinterpret<vFloat>(tmp << (10 - FRAC_BITS));
    }
    else
    {
        // Force sign to 0 (make number positive)
        vFloat exp = sfpu_exp(setsgn(in, 0));

        // Load input value, to determine whether reciprocal needs to be run
        vFloat val = dst_reg[0];

        // store tentatively e^x
        // reciprocal function relies on reloading input
        dst_reg[0] = exp;

        v_if (val < 0) {
            out = sfpu_reciprocal<true>(exp);
        }
        v_endif;
    }

    return out;
}

/*
template <bool APPROXIMATION_MODE, bool ZERO_NEGATIVE, bool SCALE_EN>
void calculate_cube(uint16_t exp_base_scale_factor = 0)
{
    for (int d = 0; d < 4; d++)
    {

        TTI_SFPLOAD(p_sfpu::LREG3, 0, 0); // load from dest
        TTI_SFPMUL(p_sfpu::LREG3, p_sfpu::LREG3, p_sfpu::LCONST_0, p_sfpu::LREG2, 0);
        TTI_NOP; TTI_NOP;
        TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LCONST_1, p_sfpu::LREG2, 0);
        TTI_NOP; TTI_NOP;
        TTI_SFPSTORE(p_sfpu::LREG2, 0, 0); // Store from lreg[1] into dest registers
        TTI_INCRWC(0, 4, 0, 0);
    }
}
*/

template <bool APPROXIMATION_MODE, bool ZERO_NEGATIVE, bool SCALE_EN>
sfpi_test_noinline void calculate_exponential(int16_t exp_base_scale_factor = 0)
{
    vFloat c23_73;
    vInt adj_exp;

    if constexpr (APPROXIMATION_MODE)
    {
        c23_73 = l_reg[LRegs::LReg0];
        adj_exp = l_reg[LRegs::LReg2];
    }

    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];
        if constexpr(SCALE_EN){
            val = val * s2vFloat16a(exp_base_scale_factor);
            // XXXX could in theory save a cycle by doing this store in the
            // shadow of the next operation.  hard to do, worth it?
            dst_reg[0] = val;
        }
        if constexpr (APPROXIMATION_MODE)
        {
            // * by 1/ln2 and add convert to 7.3 FxP format
            val = val * vConst1p4424 + c23_73;

            // Remove Exponent of 7 and bias the Mantissa to 127.
            // LREG2 already holds 2's complement value so we simply do REG2 + REG3 
            vInt val_short = adj_exp + reinterpret<vInt>(val);

            // SHL to move integer bits to exponent
            val_short <<= 10 - p_exp::FRAC_BITS;
            dst_reg[0] = reinterpret<vFloat>(val_short);

            // Needed for fused kernels such as math_row_softmax_tables which call calculate_exponential()
            // without using Relu in Packer to clamp -ve Infinity to 0.
            if constexpr (ZERO_NEGATIVE)
            {
                v_if (val_short < 0) {
                    dst_reg[0] = vConst0;
                }
                v_endif;
            }
        }
        else
        {
            // Force sign to 0 (make number positive)
            val = sfpu_exp(setsgn(val, 0));

            vFloat orig = dst_reg[0];

            // Loaded by reciprocal
            dst_reg[0] = val;
            v_if (orig < 0) {
                dst_reg[0] = sfpu_reciprocal<false>(val);
            }
            v_endif;
        }

        dst_reg++;
    }

    if constexpr (APPROXIMATION_MODE)
    {
        c23_73 = l_reg[LRegs::LReg0];
        adj_exp = l_reg[LRegs::LReg2];
    }
}

template <bool APPROXIMATION_MODE>
sfpi_inline vFloat calculate_gelu_core(vFloat in)
{
    constexpr uint imm0 = 0x18FF;
    constexpr uint imm1 = (APPROXIMATION_MODE)? 0x212C : 0x2010;
    constexpr uint imm2 = 0xFF00;

    // SFPU microcode: 
    // result = (APPROX_MODE == 1) 
    //   ? (1 + erf(x/sqrt(2)))
    //   : (1 + tanh( sqrt(2/pi) * (x + 0.044715*x^3) )
    vFloat result;
    if constexpr (APPROXIMATION_MODE) {
        result = in;
    } else {
        // f = (0.044715*x^3 + x)
        result = in * in * in;
        result = result * 0.044715f + in;

        result *= 0.79788f;
    }

    // sfpu_instr(`SFPU_LUT,0,0,0,0,3,2);
    result = lut(result, imm0, imm1, imm2);

    result = result * 0.5f + 0.5f;

    return result;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_gelu()
{
    constexpr uint imm1 = (APPROXIMATION_MODE)? 0x212C : 0x2010;
    constexpr uint imm2 = 0xFF00;
    vUInt l0;
    l0 = l_reg[LRegs::LReg0];

    // SFPU microcode
    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];
        vUInt l1;
        vUInt l2;
        vFloat result;

        if constexpr (APPROXIMATION_MODE)
        {
            l1 = imm1;
            l2 = imm2;
            result = val;
        } else {
            // f = (0.044715*x^3 + x)
            result = (val * val * val) * 0.044715f + val;

            // result = result * sqrt(2/pi)
            result *= 0.7969f;

            // Reload l1, l2 for lut
            l1 = imm1;
            l2 = imm2;
        }

        result = lut(result, l0, l1, l2);

        val = dst_reg[0];

        result = val * result + val;
        result *= 0.5f;

        dst_reg[0] = result;

        dst_reg++;
    }

    l_reg[LRegs::LReg0] = l0;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_sigmoid()
{
    // SFPU microcode
    vUInt l0, l1, l2;
    l0 = l_reg[LRegs::LReg0];
    l1 = l_reg[LRegs::LReg1];
    l2 = l_reg[LRegs::LReg2];

    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];

        val = lut(val, l0, l1, l2);

        dst_reg[0] = val + 0.5f;

        dst_reg++;
    }

    l_reg[LRegs::LReg0] = l0;
    l_reg[LRegs::LReg1] = l1;
    l_reg[LRegs::LReg2] = l2;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_tanh()
{
    // SFPU microcode
    vUInt l0, l1, l2;
    l0 = l_reg[LRegs::LReg0];
    l1 = l_reg[LRegs::LReg1];
    l2 = l_reg[LRegs::LReg2];

    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];
        val = lut(val, l0, l1, l2);
        dst_reg[0] = val;

        dst_reg++;
    }

    l_reg[LRegs::LReg0] = l0;
    l_reg[LRegs::LReg1] = l1;
    l_reg[LRegs::LReg2] = l2;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_hardtanh(uint param0, uint param1, uint param2)
{
    // All params are in FP16_B format
    // param0 = -(neg_threshold)
    // param1 = -(pos_threshold - neg_threshold)
    // param2 = -(pos_threshold)

    // XXXX move these into loop when sfpi nonimmed load perf issue is fixed
    vFloat p0 = s2vFloat16(param0);
    vFloat p1 = s2vFloat16(param1);
    vFloat p2 = s2vFloat16(param2);
    // SFPU microcode
    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];

        val += p0;// 12 bits
        v_if (val < 0.0f) {
            val = 0.0f;
        }
        v_endif;

        val += p1;// 12 bits
        v_if (val >= 0.0f) {
            val = 0.0f;
        }
        v_endif;

        val += p2;// 12 bits

        dst_reg[0] = val;

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int WITH_PRECOMPUTED_TANH>
sfpi_test_noinline void calculate_tanh_derivative()
{
    vUInt l0, l1, l2;
    l0 = l_reg[LRegs::LReg0];
    l1 = l_reg[LRegs::LReg1];
    l2 = l_reg[LRegs::LReg2];

    // tanh'(x) = 1 - (tanh(x))^2
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];

        if constexpr (!WITH_PRECOMPUTED_TANH) {
            val = lut(val, l0, l1, l2);
        }

        val = val * val;
        val = vConst1 - val;
        dst_reg[0] = val;

        dst_reg++;
    }

    l_reg[LRegs::LReg0] = l0;
    l_reg[LRegs::LReg1] = l1;
    l_reg[LRegs::LReg2] = l2;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_gelu_derivative()
{
    // SFPU microcode: 
    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];
        vFloat result = val * val * vConstNeg0p5;

        // Store intermediate result since exp(x) reloads value from dest
        dst_reg[0] = result;

        // exp = e^(val)
        vFloat exp = calculate_exponential_body<false>(result);

        // exp = exp * 1/sqrt(2*pi)
        exp *= 0.39844F;
        dst_reg[0] = exp * val;

        result = calculate_gelu_core<true>(val);

        dst_reg[0] = dst_reg[0] + result;

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
sfpi_test_noinline void calculate_reciprocal()
{
    #pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        vFloat in = dst_reg[0];
        vFloat out = sfpu_reciprocal<false, APPROXIMATION_MODE ? 2 : 3>(in);

        // Reload to reduce register pressure
        v_if (dst_reg[0] < 0.0F) {
            // Invert sign on calculated value if CC=1 (number is negative)
            out = -out;
        }
        v_endif;

        dst_reg[0] = out;

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, int RECIPROCAL_ITERATIONS=2>
sfpi_test_noinline void calculate_sqrt()
{
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];

        if constexpr (APPROXIMATION_MODE)
        {
            vUInt magic;
            magic = l_reg[LRegs::LReg2];

            //sqrt initial approximation
            // adjust bias
            vUInt val_s = magic + reinterpret<vUInt>(val);

            // approximation of square root
            val_s >>= 1;
            dst_reg[0] = reinterpret<vFloat>(val_s);

            l_reg[LRegs::LReg2] = magic;
        }
        else
        {
            // Recip root method
            //// Init approx
            //u.i = SQRT_MAGIC_F - (u.i >> 1);
            vUInt magic = reinterpret<vUInt>(vFloat(s2vFloat16b(0x5f37)));
            vFloat approx = reinterpret<vFloat>(magic - (reinterpret<vUInt>(val) >> 1));

            // Re-load to save a MOV
            val = dst_reg[0];

            //Reciproot iterations
            for (int r = 0; r < RECIPROCAL_ITERATIONS; r++)
            {
                //x*r*(1.5f - xhalf*r*r);
                approx = (approx * approx * val * vConstNeg0p5 + vConst1 + 0.5F) * approx;
            }

            dst_reg[0] = approx * val;
        }

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_dropout(uint prob, uint scale)
{
    // SFPU microcode

    FWLOG1("calculate_dropout() -- prob:%x", prob);
    FWLOG1("calculate_dropout() -- scale:%x", scale);

    vUInt rand;
    rand = l_reg[LRegs::LReg3];
    vUInt mask = reinterpret<vUInt>(vFloat(s2vFloat16b(0xa94b)));

    #pragma GCC unroll 0
    for (int d=0; d<4; d++) {
        ////////////////////////
        // Scale samples
        ///////////////////////
        dst_reg[0] = dst_reg[0] * s2vFloat16b(scale);

        ////////////////////////
        // Drop samples
        ///////////////////////
        vUInt tmp = rand >> 3;
        v_if (tmp < vUInt(prob)) {
            dst_reg[0] = vConst0;
        }
        v_endif;

        ////////////////////////
        // 16-bit PRNG update
        ///////////////////////
        tmp = rand << 1;

        // Mask = 0x593CA -> 29e4d
        // Mask = 0xd295 -> a94b
        // PRNG SHL by one
        v_if (tmp < 0) {
            rand ^= mask;
        }
        v_endif;

        dst_reg++;
    }

    l_reg[LRegs::LReg3] = rand;
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_lrelu(uint slope)
{
    // SFPU microcode
    vFloat s = s2vFloat16b(slope); // XXXX Nonimm perf workaround, hoist conversion manually

    #pragma GCC unroll 0
    for (int d=0; d<4; d++) {
        vFloat v = dst_reg[0];

        v_if (v < 0.0f) {
            v *= s;
        }
        v_endif;

        dst_reg[0] = v;

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_power(uint exponent)
{
    for (int d = 0; d < 4; d++)
    {
        vFloat in = dst_reg[0];
        vFloat result = in * in;
        for (uint i = 2; i < exponent; i++) {
            result *= in;
        }

        dst_reg[0] = result;

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_multiply()
{
    for (int d = 0; d < 4; d++)
    {
        TTI_WRCFG(p_gpr_math::DEST_OP0_BASE, 0, DEST_REGW_BASE_Base_ADDR32);
        TTI_NOP;
        TTI_SFPLOAD(2, 0, 0);	// load from dest

        TTI_WRCFG(p_gpr_math::DEST_OP1_BASE, 0, DEST_REGW_BASE_Base_ADDR32);
        TTI_NOP;
        TTI_SFPLOAD(3, 0, 0);	// load from dest

        TTI_SFPMUL(2, 3, 4, 1, 0); //lreg1 = x^2
        TTI_NOP; TTI_NOP;

        TTI_SFPSTORE(1, 0, 0);
        TTI_INCRWC(0, 4, 0, 0);
    }
}

template <bool HAS_BASE_SCALING>
sfpi_inline void calculate_log_body(const int log_base_scale_factor)
{
    ////////////////////////////
    // Load From dest + "normalize to calculation range"
    ////////////////////////////
    vFloat in = dst_reg[0];
    vFloat x = setexp(in, 127);    // set exp to exp bias (put in range of 1-2)

    ////////////////////////////
    // Calculate Cheby Approximation using Horner Form Multiplication: 3rd Order
    // x* ( x* (A*x + B) + C) + D
    // A :0.1058, B: -0.3942, C: 0.9813, D: 0.006
    // Run above on (x-1) so x is in ln(x+1), plug (x-1 into equation above to
    // save the subtract and get A',B',C',D':
    // A' = A
    // B' = -3A + B
    // C' = 3a -2B + C
    // D' = -A + B - C + D
    // A':0.1058, B':-0.7116, C':2.0871, D':-1.4753
    ////////////////////////////
    vFloat a = s2vFloat16a(0.1058F);
    vFloat series_result = x * (x * (x * a + s2vFloat16a(-0.7122f)) + s2vFloat16a(2.0869)) + s2vFloat16a(-1.4753f);

    ////////////////////////////
    // Convert exponent to float
    ////////////////////////////
    // Extract exponent and calculate abs value.  Save sign into partial reg
    vInt exp = 0;
    v_if (in != 0.0F) {
        exp = exexp(in);
        v_if (exp < 0) {
            exp = sfpi::abs(exp);
            in = setsgn(in, 1);
        }
        v_endif;
    }
    v_endif;

    // Calculate exponent of the exponent value. Done by using LZ
    // Get leading zero.  If not zero, we do 19 + ~LZ to get exponent value (mathematically == 19 - LZ - 1)
    vInt new_exp = 0;
    v_if (exp != 0) {
        new_exp = lz(exp);
        new_exp = ~new_exp;
        new_exp += 19;
        v_if (new_exp >= 0) {
            new_exp += 127;
        }
        v_endif;
    }
    v_endif;

    vFloat result = setexp(in, new_exp);
    vInt shift = lz(exp) + 1;
    result = setman(result, shft(reinterpret<vUInt>(exp), shift));
    result = result * vConst0p6929 + series_result; // exp correction: ln(1+x) + exp*ln(2)

    if constexpr (HAS_BASE_SCALING) {
        result *= s2vFloat16a(log_base_scale_factor);
    }
    
    ////////////////////////////
    // Base case when input is 0. ln(0) = -inf
    ////////////////////////////
    v_if (dst_reg[0] == 0.0F) { // Reload for register pressure
        result = -std::numeric_limits<float>::infinity();
    }
    v_endif;

    dst_reg[0] = result;

    dst_reg++;
}

template <bool APPROXIMATION_MODE, bool HAS_BASE_SCALING>
sfpi_test_noinline void calculate_log(uint log_base_scale_factor)
{
    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++) {
        calculate_log_body<HAS_BASE_SCALING>(log_base_scale_factor);
    }
}

sfpi_inline void calculate_comp_init_flag(bool check, vFloat& flag1, vFloat& flag2, float init)
{
    flag1 = init;
    if (check) {
        flag2 = init;
    }
}

template <bool APPROXIMATION_MODE, SfpuType COMP_MODE>
sfpi_test_noinline void calculate_comp(uint exponent_size_8)
{
    //invert output and use same comparison check
    constexpr bool invert_output = ((COMP_MODE == SfpuType::greater_than_equal_zero) ||
                                    (COMP_MODE == SfpuType::not_equal_zero) ||
                                    (COMP_MODE == SfpuType::greater_than_zero));

    // output_0 and output_1 hold the outputs use use when a zero or negative check is true/false.
    // False = 0.0 = kCONST_0 (5/8-bit exponent format)
    // True  = 1.0 = kCONST_1_FP16B (8-bit exponent format)
    // SFPU uses 8-bit exponent in operations so loading these constants in 8-bit exponent format.
    // Although a command flag can tell SFPU to re-bias a 5-bit exponent to 8-bit, we are loading 8-bit 
    // exponent and telling SFPU to not add any bias to these constants.
    constexpr float output_0 = invert_output ? 0.0f : 1.0f;
    constexpr float output_1 = invert_output ? 1.0f : 0.0f;

    constexpr bool check_zero = (COMP_MODE == SfpuType::equal_zero) || (COMP_MODE == SfpuType::not_equal_zero);
    constexpr bool second_check = (COMP_MODE == SfpuType::less_than_equal_zero) || (COMP_MODE == SfpuType::greater_than_zero);

    for (int d = 0; d < 4; d++)
    {
        vFloat v = dst_reg[0];
        vFloat flag1, flag2;
        if constexpr(check_zero)
        {
            v_if (sfpu_is_fp16_zero(v, exponent_size_8)) {
                calculate_comp_init_flag(second_check, flag1, flag2, output_0);
            } v_else {
                calculate_comp_init_flag(second_check, flag1, flag2, output_1);
            }
            v_endif;
        }
        else
        {
            v_if (v < 0.0F) {
                calculate_comp_init_flag(second_check, flag1, flag2, output_0);
            } v_else {
                calculate_comp_init_flag(second_check, flag1, flag2, output_1);
            }
            v_endif;
        }

        vFloat result;
        if constexpr (second_check)
        {
            // SfpuType::less_than_equal_zero
            // flag1 = 0x3F80(1.0) if DST < 0 else 0
            // flag2 = 0x3F80(1.0) if DST == 0 else 0
            // Do a bitwise Or (flag1 | flag2) to get <= condition.
            // flag1 < 0 OR flag2 == 0 => DST is Less than or Equal to zero.
            // Result will be either 0x0000(0.0) or 0x3F80(1.0)
            if constexpr (COMP_MODE == SfpuType::less_than_equal_zero) {
                result = reinterpret<vFloat>(reinterpret<vUInt>(flag1) | reinterpret<vUInt>(flag2));
            }
            else
            {
                // SfpuType::greater_than_zero
                // flag1 = 0x3F80(1.0) if DST >= 0 else 0
                // flag2 = 0x3F80(1.0) if DST != 0 else 0
                // Do a bitwise And (flag1 & flag2) to get > condition.
                // flag2 >= 0 AND flag1 != 0 => DST is Greater than zero 
                // Result will be either 0x0000(0.0) or 0x3F80(1.0)
                result = reinterpret<vFloat>(reinterpret<vUInt>(flag1) & reinterpret<vUInt>(flag2));
            }
        } else {
            result = flag1;
        }

        dst_reg[0] = result;
        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_clamp(uint param0, uint param1, uint param2)
{
    // All params are in FP16 format
    // param0 = min
    // param1 = max

    //uint format = (param0 >> 16)&0x1;
    s2vFloat16::Format format = s2vFloat16::fp16a;

    // SFPU microcode
    vFloat min = s2vFloat16(param0, format);
    vFloat max = s2vFloat16(param1, format);
    #pragma GCC unroll 0
    for (int d = 0; d < 4; d++)
    {
        vFloat val = dst_reg[0];

        v_if (val < min) {
            val = s2vFloat16(param0, format);
        } v_elseif (val >= max) {
            val = s2vFloat16(param1, format);
        }
        v_endif;

        dst_reg[0] = val + s2vFloat16b(param2); // 12 bits

        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_abs()
{
    // SFPU microcode
    for (int d = 0; d < 4; d++)
    {
        vFloat v = dst_reg[0];
        dst_reg[0] = sfpi::abs(v);
        dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
sfpi_test_noinline void calculate_sign(uint exponent_size_8)
{
    // All params are in FP16 format
    // uint format = 1;
    for (int d = 0; d < 4; d++)
    {
        vFloat v = dst_reg[0];
        dst_reg[0] = vConst1;
        v_if (v < 0.0F) {
            dst_reg[0] = vConstNeg1;
        }
        v_endif;

        //param0 == 0 is Bfp8 format. It does not require bias removal.
        //param0 != 0 is Float16 format and exp bias needs to be removed for zero check.
        v_if (sfpu_is_fp16_zero(v, exponent_size_8)) {
            dst_reg[0] = vConst0;
        }
        v_endif;

        dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel

using namespace ckernel::sfpu;

int main(int argc, char* argv[])
{
    init_dropout_seed(argc);

    calculate_comp<false, SfpuType::greater_than_equal_zero>(argc);
    calculate_comp<false, SfpuType::not_equal_zero>(argc);
    calculate_comp<false, SfpuType::greater_than_zero>(argc);
    calculate_comp<false, SfpuType::less_than_equal_zero>(argc);
    calculate_comp<false, SfpuType::equal_zero>(argc);

    calculate_clamp<false>(argc, argc, argc);
    calculate_abs<false>();
    calculate_sign<false>(true);

    calculate_sigmoid<false>();
    calculate_tanh<true>();
    calculate_hardtanh<false>(argc, argc, argc);
    calculate_lrelu<false>(argc);
    calculate_tanh_derivative<false, false>();
    calculate_tanh_derivative<false, true>();
    calculate_power<true>(argc);

    calculate_gelu<false>();
    calculate_gelu<true>();
    calculate_gelu_derivative<false>();

    calculate_sqrt<false, 4>();
    calculate_sqrt<true, 4>();
    calculate_exponential<false, false, false>(0);
    calculate_exponential<false, false, true>(0);
    calculate_exponential<false, true, false>(0);
    calculate_exponential<false, true, true>(0);
    calculate_exponential<true, false, false>(0);
    calculate_exponential<true, false, true>(0);
    calculate_exponential<true, true, false>(0);
    calculate_exponential<true, true, true>(0);
    calculate_log<true, true>(argc);
    calculate_log<true, false>(argc);

    calculate_reciprocal<false, 4>();
    calculate_reciprocal<true, 4>();

    calculate_dropout<false>(argc, argc);
}
