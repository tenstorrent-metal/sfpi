/*
 * SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Test cases for esoteric liveness.  See liveness word doc
#include <stdio.h>
#include <stdlib.h>
#include "test.h"

using namespace sfpi;

void abs_setcc()
// ABS liveness across SETCC
{
    vFloat x = -20.0F;
    vFloat y = -30.0F;
    v_if (dst_reg[0] == 0.0F) {
        y = abs(x);
    }
    v_endif;
    dst_reg[13] = y;
}

// Assignment resulting in register rename
void rename_move_case2a()
{
    vFloat a = 1.0f;  // LOADI
    vFloat b = 2.0f;  // LOADI
    v_if (dst_reg[0] == 0.0f) {  // PUSHC, ..., SETCC
        b = a;         // should generate MOV
    }
    v_endif;           // POPC
    dst_reg[0] = b;      // STORE
}

// Assignment requiring move
// This straddles case 2a and 3 - both values need to be preserved but the
// compiler doesn't know that, solving case2a will solve this case as well
void copy_move_case2b()
{
    vFloat a = 1.0f;  // LOADI
    vFloat b = 2.0f;  // LOADI
    v_if (dst_reg[0] == 0.0f) {  // PUSHC, ..., SETCC
        b = a;         // should generate MOV
    }
    v_endif;           // POPC
    dst_reg[0] = a; // STORE
    dst_reg[0] = b; // STORE
}

// Assignment requiring move (both a and b need to be preserved)
void copy_move_case3()
{
    vFloat a = 1.0f;  // LOADI
    vFloat b = 2.0f;  // LOADI
    v_if (dst_reg[0] == 0.0f) {  // PUSHC, ..., SETCC
        b = a;         // should generate MOV
    }
    v_endif;           // POPC
    dst_reg[0] = a + 3.0f; // STORE
    dst_reg[0] = b + 4.0f; // STORE
}

// Destination as source, 2 arguments in the wrong order
void internal_move_case4()
{
    vInt a = 10; // LOADI
    vInt b = 20; // LOADI
    v_if (dst_reg[0] == 0.0f) {      // PUSHC, ..., SETCC
        a = a - b;   // CCMOV, IADD
        // Wrapper emits:
        //   tmp = b;
        //   tmp = a - tmp;
        //   a = tmp;
    }
    v_endif;         // POPC
    dst_reg[0] = a;    // STORE
    dst_reg[0] = b;    // STORE
}

// Destination as source 3 arguments
void internal_move_case5()
{
    vInt a = 10; // LOADI
    vInt b = 20; // LOADI
    vInt c = 30; // LOADI
    v_if (dst_reg[0] == 0.0f) {      // PUSHC, ..., SETCC
        c = a - b;   // MOV, IADD

        // Wrapper emits:
        //   tmp = b;
        //   tmp = a - tmp;
        //   c = tmp;

        // really
        // c = b
        // c = a - c
    }
    v_endif;         // POPC
    dst_reg[0] = a;    // STORE
    dst_reg[0] = b;    // STORE
    dst_reg[0] = c;    // STORE
}

void bb_1()
{
    vInt a = 1;
    vInt b = 2;

    for (int i = 0; i < rand(); i++) {
        v_if (a >= 3) {
            if (rand()) {
                continue;
            }
            a = b + 4;
        } v_elseif (a >= 5) {
            if (rand()) {
                break;
            }
            a = b + 6;
        }
        v_endif;
    }

    dst_reg[0] = a;
}

void bb_2()
{
    vInt a = 1;
    vInt b = 2;

    for (int i = 0; i < rand(); i++) {
        v_block {
            switch (i) {
            case 0:
                a = b - 3;
                break;
            case 1:
                v_and(a >= 4);
                break;
            default:
                break;
            }

            // Use a builtin...
            a = b - 5;
        }

        v_endblock;
    }

    dst_reg[0] = a;
}

void bb_3()
{
    vInt a = 1;
    vInt b = 2;

    for (int i = 0; i < rand(); i++) {
        if (rand() == 0) {
            goto target2;
        }

        v_if (a >= 3) {
            if (rand() > 2) {
                goto target1;
            }
            a = b - 4;
        } v_elseif(a >= 5) {
        target1:
            a = b - 6;
        }
        v_endif;
    }
 target2:

    dst_reg[0] = a;
}

void bb_4()
{
    vInt a = 1;
    vInt b = 2;

    v_block {

        v_and(b >= 1);

        if (rand() == 5)
            goto seq1;

        if (rand() == 6)
            goto seq2;

        if (rand() == 6)
            goto seq2;

    seq1:
        v_and(b >= 2);
        goto join;

    seq2:
        v_and(b >= 3);
        v_and(b >= 4);
        
    join:
        a = 5;
    }
    v_endif;

    dst_reg[0] = a;
}

// Test cleaning up args in PHI nodes after deleting the live assignment
void bb_not_live()
{
    vInt a = 1;
    vInt b = 2;

    for (int i = 0; i < rand(); i++) {
        a = b - 3;
    }

    a = b - 4;

    dst_reg[0] = a;
}

// Liveness pass (used to) handle unrolling the POPC loop
void popc_unrolling()
{
    vInt a = 1;
    vInt b = 1;

    v_if (a >= 2) {
        a = b - 1;
    } v_elseif (a >= 3) {
        a = b - 1;
    } v_elseif (a >= 4) {
        a = b - 1;
    } v_elseif (a >= 5) {
        a = b - 1;
    }
    v_endif;

    v_if (a >= 6) {
        a = b - 1;
    } v_elseif (a >= 7) {
        a = b - 1;
    } v_elseif (a >= 8) {
        a = b - 1;
    }
    v_endif;

    dst_reg[0] = a;
}


// This tests to be sure that 2 assignments at a deeper CC level both work,
// ie, that the 2nd one doesn't find the prior assignment and use its
// liveness, but continues up the chain to the origin
void double_assign_even()
{
    vInt a = 1;
    vInt b = 2;

    v_if (a == 3) {
        a = b & a;
        vInt b = 5;
        vInt c = 7;
        vInt d = 8;
        dst_reg[0] = b;
        dst_reg[0] = c;
        dst_reg[0] = d;

        a = ~a;
    }
    v_endif;

    dst_reg[0] = a;
}

// The code below tests the case where we descend down a CC cascade, pop
// back up, then back down w/ different CC bits set.  Does the variable
// stay live when assigned at the same CC level but in a different
// cascade, ie, across generations?
void generation()
{
    vInt a;
    vInt b;
    vInt dr = 1;

    v_if (dr == 2.0 || dr == 3.0) {
        b = 4;
    }
    v_endif;

    v_if (dr == 5.0) {
        b = -6;
    }
    v_endif;

    v_if (dr == 7) {
        a = 8;
    }
    v_endif;

    v_if (dr == 9) {
        a = 10;
    }
    v_endif;

    v_if (dr == 11 || dr == 12) {
        dst_reg[14] = b;
    }
    v_endif;

    v_if (dr == 13) {
        dst_reg[14] = a;
    }
    v_endif;
}

// Can liveness be determined when there is an intervening pseudo live insn
void prop_thru_pseudo_live()
{
    vInt a = 1;
    vInt b = 2;

    v_if (a == 4) {
        a = b & a;
        a = b | a;
        a = b + a;

        a = a & b;
        a = a | b;
        a = a + b;
    }
    v_endif;

    dst_reg[0] = a;
}

void do_not_fold_assign()
{
    vInt a = 1;
    vInt b = 2;

    v_if (a < 0) {
        a = b;
    }
    v_endif;

    dst_reg[0] = a;
}

int main(int argc, char* argv[])
{
    abs_setcc();
    rename_move_case2a();
    copy_move_case2b();
    copy_move_case3();
    internal_move_case4();
    internal_move_case5();
    bb_1();
    bb_2();
    bb_3();
    bb_4();
    bb_not_live();
    popc_unrolling();
    double_assign_even();
    generation();
    prop_thru_pseudo_live();
    do_not_fold_assign();
}
