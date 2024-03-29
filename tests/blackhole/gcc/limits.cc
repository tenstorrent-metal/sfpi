/*
 * SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
typedef float v64sf __attribute__((vector_size(64*4)));

// Ensures that there is no prologue/epilogue in the generated assembly.
void limits()  __attribute__((naked));

#ifndef LIMIT_OFFSET
#define LIMIT_OFFSET 0
#endif

#ifndef PASS_OFFSET
#define PASS_OFFSET LIMIT_OFFSET
#endif

#ifndef FAIL_OFFSET
#define FAIL_OFFSET LIMIT_OFFSET
#endif

constexpr int pass_offset = PASS_OFFSET;
constexpr int fail_offset = FAIL_OFFSET;

int main(int argc, char* argv[])
{
    v64sf a = __builtin_rvtt_bh_sfpload(nullptr, 0, 0, 0x0000, 0, 0);

    a = __builtin_rvtt_bh_sfpload(nullptr, 0, 7, 0x0000 + fail_offset, 0, 0);
    a = __builtin_rvtt_bh_sfpload(nullptr, 0, 0, 0x1FFF + fail_offset, 0, 0);
    a = __builtin_rvtt_bh_sfpload(nullptr, 0, 0, fail_offset, 0, 0);

    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 7, 0x0000 + fail_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0x1FFF + fail_offset, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 2, 0x0000 + fail_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 2, 0xFFFF + fail_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Signed 16 bit
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 4, -32768 + fail_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 4,  32767 + fail_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Signed 16-32 bit crossover
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 16, -32768 + pass_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 16,  32767 + pass_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 16,  65535 + pass_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Unsigned 16-32 bit crossover
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 17,  0 + pass_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 17,  0xFFFF + pass_offset, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Float values
    // Easy fp16b
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3f800000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // fp16b
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3faf0000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // 32 bit float
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3faf0001, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // fp16a
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3fa6e000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // 32 bit float
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3fa6e001, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Exp on the edge of fp16a
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x47a6e000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // 32 bit float
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x4826e000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Exp on the edge of fp16a
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x3826e000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // 32 bit float
    a = __builtin_rvtt_bh_sfpxloadi(nullptr, 18,  0x37a6e000, 0, 0);
    __builtin_rvtt_bh_sfpstore(nullptr, a, 0, 0, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_bh_sfpaddi(nullptr, a, 0x0000 + fail_offset, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpaddi(nullptr, a, 0xFFFF + fail_offset, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_bh_sfpmuli(nullptr, a, 0x0000 + fail_offset, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpmuli(nullptr, a, 0xFFFF + fail_offset, 0, 0, 0);

    // Signed 12 bit
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, 0x07FF + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, -1 + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, -2048 + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, 0x07FF + pass_offset, 0, 0, a, 1);
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, -1 + pass_offset, 0, 0, a, 1);
    a = __builtin_rvtt_bh_sfpdivp2(nullptr, -2048 + pass_offset, 0, 0, a, 1);

    // Signed 12 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 2047 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, -1 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, -2048 + pass_offset, 0, 0, 8);

    // Signed 16 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 32767 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, -1 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, -32768 + pass_offset, 0, 0, 8);

    // Signed 32 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 65535 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, -65536 + pass_offset, 0, 0, 8);

    // Unsigned 12 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 0 + pass_offset, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 4095 + pass_offset, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 0 + pass_offset, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 65535 + pass_offset, 0, 0, 0);

    // Unsigned 32 bit
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 65535 + pass_offset, 0, 0, 0);
    a = __builtin_rvtt_bh_sfpxiadd_i(nullptr, a, 65536 + pass_offset, 0, 0, 0);

    // Signed 12 bit
    a = __builtin_rvtt_bh_sfpshft_i(nullptr, a, 0x07FF + pass_offset, 0, 0);
    a = __builtin_rvtt_bh_sfpshft_i(nullptr, a, -1 + pass_offset, 0, 0);
    a = __builtin_rvtt_bh_sfpshft_i(nullptr, a, -2048 + pass_offset, 0, 0);

    // Unsigned 12 bit
    a = __builtin_rvtt_bh_sfpsetexp_i(nullptr, 0 + pass_offset, 0, 0, a);
    a = __builtin_rvtt_bh_sfpsetexp_i(nullptr, 0x0FFF + pass_offset, 0, 0, a);

    // Unsigned 12 bit
    a = __builtin_rvtt_bh_sfpsetman_i(nullptr, 0 + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_bh_sfpsetman_i(nullptr, 0x0FFF + pass_offset, 0, 0, a, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_bh_sfpsetman_i(nullptr, 0x1000 + pass_offset, 0, 0, a, 0);

    // Unsigned 12 bit
    a = __builtin_rvtt_bh_sfpsetsgn_i(nullptr, 0 + pass_offset, 0, 0, a);
    a = __builtin_rvtt_bh_sfpsetsgn_i(nullptr, 0x0FFF + pass_offset, 0, 0, a);

    // Unsigned 1 bit.  Not masked in compiler but not user visible
    __builtin_rvtt_bh_sfppushc(0);
    __builtin_rvtt_bh_sfpsetcc_i(0, 1);
    __builtin_rvtt_bh_sfpsetcc_i(1, 1);
    __builtin_rvtt_bh_sfppopc(0);

    // Unsigned 1 bit.  Not masked in compiler but not user visible
    __builtin_rvtt_bh_sfpencc(0, 2);
    __builtin_rvtt_bh_sfpencc(1, 2);

    int cond;
    __builtin_rvtt_bh_sfppushc(0);
    // 1.0 in different fmts
    int top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x3f80, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x3f80, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x3f800000, 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);

    // -1.0 in different fmts
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbf80, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbf80, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbf800000, 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);

    // Not a register in different formats
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbfa0, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbfa0, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0xbfa00000, 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x3fa6e001, 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);

    // Test limits of fp16a/fp16b/float32 determination
    // This is fp16b, with pass_offset != 0 the mantissa will overflow, use fp16a
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x3fff0000 | ((pass_offset & 1) << 15), 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);
    // This is fp16b w/ large exp, with pass_offset != 0 the mantissa will overflow, use fp32
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x47ff0000 | ((pass_offset & 1) << 15), 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);
    // This is fp16a w/ largest exp, with pass_offset != 0 the exponent will overflow, use fp32
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x477f8000 | ((pass_offset & 1) << 23), 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);
    // This is fp16a w/ smallest exp, with pass offset != 0 the exponent will underflow, use fp32
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_bh_sfpxfcmps(nullptr, a, 0x38ff8000 & ~((pass_offset & 1) << 23), 0, 0, 33);
    __builtin_rvtt_sfpxcondb(cond, top);

    __builtin_rvtt_bh_sfppopc(0);
}
