typedef float v64sf __attribute__((vector_size(64*4)));

// Ensures that there is no prologue/epilogue in the generated assembly.
void smoke()  __attribute__((naked));
void smokier()  __attribute__((naked));

constexpr unsigned int SFPMOV_MOD1_COMPSIGN = 1;
constexpr unsigned int SFPABS_MOD1_FLOAT = 1;
constexpr unsigned int SFPEXEXP_MOD1_DEBIAS = 0;
constexpr unsigned int SFPEXEXP_MOD1_NODEBIAS = 1;
constexpr unsigned int SFPEXMAN_MOD1_PAD8 = 1;


void smoke()
{
    v64sf a, b, c, d;

    a = __builtin_riscv_sfpload(nullptr, 1, 0);
    b = __builtin_riscv_sfpload(nullptr, 1, 16);
    c = __builtin_riscv_sfpload(nullptr, 1, 32);

    v64sf lr6, lr7, lr8;
    lr6 = __builtin_riscv_sfpassignlr(6);
    lr7 = __builtin_riscv_sfpassignlr(7);
    lr8 = __builtin_riscv_sfpassignlr(8);

    d = __builtin_riscv_sfpmad(  a,   b,   c, 1);
    d = __builtin_riscv_sfpmad(  a,   b, lr6, 1);
    d = __builtin_riscv_sfpmad(  a, lr7,   c, 1);
    d = __builtin_riscv_sfpmad(  a, lr7, lr6, 1);
    d = __builtin_riscv_sfpmad(lr8,   b,   c, 1);
    d = __builtin_riscv_sfpmad(lr8,   b, lr6, 1);
    d = __builtin_riscv_sfpmad(lr8, lr7,   c, 1);
    d = __builtin_riscv_sfpmad(lr8, lr7, lr6, 1);

    d = __builtin_riscv_sfpmul(lr8, lr7, 1);
    d = __builtin_riscv_sfpadd(lr8, lr7, 1);

    __builtin_riscv_sfpencc(2, 8);

    __builtin_riscv_sfppushc();
    __builtin_riscv_sfpsetcc_v(a, 12);
    __builtin_riscv_sfpsetcc_i(3, 12);
    __builtin_riscv_sfpcompc();
    __builtin_riscv_sfppopc();

    a = __builtin_riscv_sfploadi(nullptr, 0, 12);
    a = __builtin_riscv_sfploadi(nullptr, 0, -12);
    __builtin_riscv_sfpstore(nullptr, a, 2, 0);
    __builtin_riscv_sfpstore(nullptr, a, 2, 4);
    v64sf lr13 = __builtin_riscv_sfpassignlr(13);
    __builtin_riscv_sfpstore(nullptr, lr13, 2, 4);
    v64sf e = __builtin_riscv_sfpmad(a, b, c, 1);
    __builtin_riscv_sfpstore(nullptr, e, 2, 4);

    __builtin_riscv_sfpnop();

    v64sf v1, v2;
    __builtin_riscv_sfpstore(nullptr, b, 2, 1);
    v1 = __builtin_riscv_sfpload(nullptr, 2, 0);
    v2 = __builtin_riscv_sfpmov(v1, SFPMOV_MOD1_COMPSIGN);

    __builtin_riscv_sfpstore(nullptr, c, 2, 2);
    __builtin_riscv_sfpstore(nullptr, d, 2, 3);
    __builtin_riscv_sfpstore(nullptr, v2, 2, 10);

    v2 = __builtin_riscv_sfpexexp(v1, SFPEXEXP_MOD1_DEBIAS);
    v2 = __builtin_riscv_sfpexman(v1, SFPEXMAN_MOD1_PAD8);

    v2 = __builtin_riscv_sfpsetexp_i(nullptr, 23, v1);
    v2 = __builtin_riscv_sfpsetexp_v(v2, v1);

    v2 = __builtin_riscv_sfpsetman_i(nullptr, 23, v1);
    v2 = __builtin_riscv_sfpsetman_v(v2, v1);

    v2 = __builtin_riscv_sfpabs(v1, SFPABS_MOD1_FLOAT);
    v2 = __builtin_riscv_sfpand(v2, v1);

    v2 = __builtin_riscv_sfpor(v2, v1);
    v2 = __builtin_riscv_sfpnot(v1);

    v2 = __builtin_riscv_sfpmuli(nullptr, v2, 32, 1);
    v2 = __builtin_riscv_sfpaddi(nullptr, v2, 32, 0);

    v2 = __builtin_riscv_sfpdivp2(nullptr, 32, v1, 1);

    v2 = __builtin_riscv_sfplz(v1, 2);

    v2 = __builtin_riscv_sfpshft_i(nullptr, v2, 10);
    v2 = __builtin_riscv_sfpshft_v(v2, v1);

    v1 = __builtin_riscv_sfpiadd_i(nullptr, v1, 10, 4);
    v2 = __builtin_riscv_sfpiadd_v(v2, v1, 8);

    v2 = __builtin_riscv_sfpsetsgn_i(nullptr, 10, v1);
    v2 = __builtin_riscv_sfpsetsgn_v(v2, v1);

    __builtin_riscv_sfpstore(nullptr, v1, 2, 6);
    __builtin_riscv_sfpstore(nullptr, v2, 2, 8);

    v2 = __builtin_riscv_sfpiadd_v(v2, lr13, 4);

    v2 = __builtin_riscv_sfplut(v2, d, c, v1, 1);
}

void smoke_live()
{
    v64sf a, d;

    a = __builtin_riscv_sfpload(nullptr, 1, 0);
    a = __builtin_riscv_sfpload_lv(nullptr, a, 1, 0);

    d = __builtin_riscv_sfpload(nullptr, 1, 0);
    d = __builtin_riscv_sfpmad_lv(d,   a,   a,   a, 1);
    d = __builtin_riscv_sfpmul_lv(d,   a,   a, 1);
    d = __builtin_riscv_sfpadd_lv(d,   a,   a, 1);

    a = __builtin_riscv_sfploadi_lv(nullptr, a, 0, 12);

    a = __builtin_riscv_sfpmov_lv(a, d, SFPMOV_MOD1_COMPSIGN);

    a = __builtin_riscv_sfpexexp_lv(a, d, SFPEXEXP_MOD1_DEBIAS);
    a = __builtin_riscv_sfpexman_lv(a, d, SFPEXMAN_MOD1_PAD8);

    a = __builtin_riscv_sfpsetexp_i_lv(nullptr, a, 23, d);
    a = __builtin_riscv_sfpsetman_i_lv(nullptr, a, 23, d);

    a = __builtin_riscv_sfpabs_lv(a, d, SFPABS_MOD1_FLOAT);
    a = __builtin_riscv_sfpnot_lv(a, d);

    a = __builtin_riscv_sfpdivp2_lv(nullptr, a, 32, d, 1);

    a = __builtin_riscv_sfplz_lv(a, d, 2);

    a = __builtin_riscv_sfpiadd_i_lv(nullptr, a, d, 10, 4);

    a = __builtin_riscv_sfpsetsgn_i_lv(nullptr, a, 10, d);
}

void smokier()
{
    v64sf a = __builtin_riscv_sfpassignlr(2);
    v64sf b = __builtin_riscv_sfpassignlr(0);
    v64sf c = __builtin_riscv_sfpassignlr(1);
    v64sf d = __builtin_riscv_sfpload(nullptr, 1, 20);

    d = __builtin_riscv_sfpmad(a, b, c, 3);
    d = __builtin_riscv_sfplut(b, c, a, d, 7);

    __builtin_riscv_sfpstore(nullptr, a, 2, 20);
    __builtin_riscv_sfpstore(nullptr, b, 2, 20);
    __builtin_riscv_sfpstore(nullptr, c, 2, 20);
    __builtin_riscv_sfpstore(nullptr, d, 2, 20);

    v64sf e = __builtin_riscv_sfpload(nullptr, 1, 20);
    v64sf f = __builtin_riscv_sfpload(nullptr, 1, 20);
    v64sf g = __builtin_riscv_sfpload(nullptr, 1, 20);

    __builtin_riscv_sfpstore(nullptr, e, 2, 20);
    __builtin_riscv_sfpstore(nullptr, f, 2, 20);
    __builtin_riscv_sfpstore(nullptr, g, 2, 20);
    __builtin_riscv_sfpkeepalive(c, 2);
}

void smokiest()
{
    v64sf a, b, orig_a;

    a = __builtin_riscv_sfpload(nullptr, 2, 0);
    b = __builtin_riscv_sfpload(nullptr, 2, 1);

    orig_a = a;
    a = b;
    a = __builtin_riscv_sfpsetexp_v(a, orig_a);
   
    __builtin_riscv_sfpstore(nullptr, a, 2, 6);
}


void assignlr()
{
    v64sf x;

    x = __builtin_riscv_sfpassignlr(0);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(1);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(2);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(3);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(4);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(5);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(6);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(7);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(8);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(9);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(10);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(11);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(12);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(13);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(14);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
    x =__builtin_riscv_sfpassignlr(15);
    __builtin_riscv_sfpstore(nullptr, x, 0, 0);
}

int main(int argc, char* argv[])
{
    assignlr();
    smoke();
    smoke_live();
    smokier();
    smokiest();
}