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
    v64sf a = __builtin_rvtt_gs_sfpload(nullptr, 0, 0x0000, 0, 0);

    a = __builtin_rvtt_gs_sfpload(nullptr, 0, 0x0000 + fail_offset, 0, 0);
    a = __builtin_rvtt_gs_sfpload(nullptr, 0, 0xFFFF + fail_offset, 0, 0);
    a = __builtin_rvtt_gs_sfpload(nullptr, 0, fail_offset, 0, 0);

    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0x0000 + fail_offset, 0, 0);
    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0xFFFF + fail_offset, 0, 0);

    // Signed 16 bit
    a = __builtin_rvtt_gs_sfpxloadi(nullptr, 2, 0x0000 + fail_offset, 0, 0);
    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpxloadi(nullptr, 2, 0xFFFF + fail_offset, 0, 0);
    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpxloadi(nullptr, 4, -32768 + fail_offset, 0, 0);
    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpxloadi(nullptr, 4,  32767 + fail_offset, 0, 0);
    __builtin_rvtt_gs_sfpstore(nullptr, a, 0, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_gs_sfpaddi(nullptr, a, 0x0000 + fail_offset, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpaddi(nullptr, a, 0xFFFF + fail_offset, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_gs_sfpmuli(nullptr, a, 0x0000 + fail_offset, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpmuli(nullptr, a, 0xFFFF + fail_offset, 0, 0, 0);

    // Signed 12 bit
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, 0x07FF + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, -1 + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, -2048 + pass_offset, 0, 0, a, 0);
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, 0x07FF + pass_offset, 0, 0, a, 1);
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, -1 + pass_offset, 0, 0, a, 1);
    a = __builtin_rvtt_gs_sfpdivp2(nullptr, -2048 + pass_offset, 0, 0, a, 1);

    // Signed 12 bit
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 2047 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, -1 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, -2048 + pass_offset, 0, 0, 8);

    // Signed 16 bit
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 32767 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, -1 + pass_offset, 0, 0, 8);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, -32768 + pass_offset, 0, 0, 8);

    // Unsigned 12 bit
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 0 + pass_offset, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 4095 + pass_offset, 0, 0, 0);

    // Unsigned 16 bit
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 0 + pass_offset, 0, 0, 0);
    a = __builtin_rvtt_gs_sfpxiadd_i(nullptr, a, 65535 + pass_offset, 0, 0, 0);

    // Signed 12 bit
    a = __builtin_rvtt_gs_sfpshft_i(nullptr, a, 0x07FF + pass_offset, 0, 0);
    a = __builtin_rvtt_gs_sfpshft_i(nullptr, a, -1 + pass_offset, 0, 0);
    a = __builtin_rvtt_gs_sfpshft_i(nullptr, a, -2048 + pass_offset, 0, 0);

    // Unsigned 12 bit
    a = __builtin_rvtt_gs_sfpsetexp_i(nullptr, 0 + pass_offset, 0, 0, a);
    a = __builtin_rvtt_gs_sfpsetexp_i(nullptr, 0x0FFF + pass_offset, 0, 0, a);

    // Unsigned 12 bit
    a = __builtin_rvtt_gs_sfpsetman_i(nullptr, 0 + pass_offset, 0, 0, a);
    a = __builtin_rvtt_gs_sfpsetman_i(nullptr, 0x0FFF + pass_offset, 0, 0, a);

    // Unsigned 12 bit
    a = __builtin_rvtt_gs_sfpsetsgn_i(nullptr, 0 + pass_offset, 0, 0, a);
    a = __builtin_rvtt_gs_sfpsetsgn_i(nullptr, 0x0FFF + pass_offset, 0, 0, a);

    // Unsigned 1 bit.  Not masked in compiler but not user visible
    __builtin_rvtt_gs_sfppushc();
    __builtin_rvtt_gs_sfpsetcc_i(0, 1);
    __builtin_rvtt_gs_sfpsetcc_i(1, 1);
    __builtin_rvtt_gs_sfppopc();

    // Unsigned 1 bit.  Not masked in compiler but not user visible
    __builtin_rvtt_gs_sfpencc(0, 2);
    __builtin_rvtt_gs_sfpencc(1, 2);

    int cond;
    __builtin_rvtt_gs_sfppushc();
    // 1.0 in different fmts
    int top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0x3f80, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0x3f80, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);

    // -1.0 in different fmts
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0xbf80, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0xbf80, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);

    // Not a register in different formats
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0xbfa0, 0, 0, 9);
    __builtin_rvtt_sfpxcondb(cond, top);
    top = __builtin_rvtt_sfpxvif();
    cond = __builtin_rvtt_gs_sfpxfcmps(nullptr, a, 0xbfa0, 0, 0, 17);
    __builtin_rvtt_sfpxcondb(cond, top);

    __builtin_rvtt_gs_sfppopc();
}
