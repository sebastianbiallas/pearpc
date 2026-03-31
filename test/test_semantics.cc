/*
 *	Test harness for abstract PPC semantics and liveness analysis.
 *	Pure header-only — no CPU library needed.
 *
 *	Build:  make -C test test_semantics
 *	Run:    test/test_semantics
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Minimal types needed by the headers
#include "system/types.h"

#include "cpu/ppc_liveness.h"
#include "cpu/ppc_semantics_dispatch.h"
#include "cpu/ppc_concrete_sem.h"

static int failures = 0;
static int tests = 0;

#define CHECK(desc, cond)                                                                                              \
    do {                                                                                                               \
        tests++;                                                                                                       \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "FAIL: %s\n", desc);                                                                       \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

// Helper to build PPC opcodes
static uint32 ppc_addi(int rD, int rA, int imm)
{
    return (14u << 26) | (rD << 21) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_addis(int rD, int rA, int imm)
{
    return (15u << 26) | (rD << 21) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_ori(int rS, int rA, int imm)
{
    return (24u << 26) | (rS << 21) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_add(int rD, int rA, int rB, bool rc = false)
{
    return (31u << 26) | (rD << 21) | (rA << 16) | (rB << 11) | (266 << 1) | (rc ? 1 : 0);
}

static uint32 ppc_subf(int rD, int rA, int rB, bool rc = false)
{
    return (31u << 26) | (rD << 21) | (rA << 16) | (rB << 11) | (40 << 1) | (rc ? 1 : 0);
}

static uint32 ppc_cmpi(int crfD, int rA, int imm)
{
    return (11u << 26) | (crfD << 23) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_cmpli(int crfD, int rA, int imm)
{
    return (10u << 26) | (crfD << 23) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_rlwinm(int rS, int rA, int SH, int MB, int ME, bool rc = false)
{
    return (21u << 26) | (rS << 21) | (rA << 16) | (SH << 11) | (MB << 6) | (ME << 1) | (rc ? 1 : 0);
}

static uint32 ppc_or(int rS, int rA, int rB, bool rc = false)
{
    return (31u << 26) | (rS << 21) | (rA << 16) | (rB << 11) | (444 << 1) | (rc ? 1 : 0);
}

static uint32 ppc_and(int rS, int rA, int rB, bool rc = false)
{
    return (31u << 26) | (rS << 21) | (rA << 16) | (rB << 11) | (28 << 1) | (rc ? 1 : 0);
}

static uint32 ppc_srawi(int rS, int rA, int SH)
{
    return (31u << 26) | (rS << 21) | (rA << 16) | (SH << 11) | (824 << 1);
}

static uint32 ppc_subfic(int rD, int rA, int imm)
{
    return (8u << 26) | (rD << 21) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_addic(int rD, int rA, int imm)
{
    return (12u << 26) | (rD << 21) | (rA << 16) | (imm & 0xffff);
}

static uint32 ppc_crand(int crD, int crA, int crB)
{
    return (19u << 26) | (crD << 21) | (crA << 16) | (crB << 11) | (257 << 1);
}

static uint32 ppc_crxor(int crD, int crA, int crB)
{
    return (19u << 26) | (crD << 21) | (crA << 16) | (crB << 11) | (193 << 1);
}

// ---- InsnEffect tests ----

static void test_insn_effect_addi()
{
    // addi r3, r4, 5
    InsnEffect fx = ppc_analyze_insn(ppc_addi(3, 4, 5));
    CHECK("addi reads r4", fx.gpr_read & (1u << 4));
    CHECK("addi writes r3", fx.gpr_write & (1u << 3));
    CHECK("addi doesn't read r3", !(fx.gpr_read & (1u << 3)));
    CHECK("addi doesn't write r4", !(fx.gpr_write & (1u << 4)));
    CHECK("addi no CR write", fx.cr_write == 0);
    CHECK("addi no XER write", fx.xer_write == 0);
}

static void test_insn_effect_addi_r0()
{
    // addi r3, r0, 5 — rA=0 means literal zero, NOT gpr[0]
    InsnEffect fx = ppc_analyze_insn(ppc_addi(3, 0, 5));
    CHECK("addi r0 doesn't read r0", !(fx.gpr_read & (1u << 0)));
    CHECK("addi r0 writes r3", fx.gpr_write & (1u << 3));
}

static void test_insn_effect_add()
{
    // add r5, r3, r4
    InsnEffect fx = ppc_analyze_insn(ppc_add(5, 3, 4));
    CHECK("add reads r3", fx.gpr_read & (1u << 3));
    CHECK("add reads r4", fx.gpr_read & (1u << 4));
    CHECK("add writes r5", fx.gpr_write & (1u << 5));
    CHECK("add no CR write (no Rc)", fx.cr_write == 0);
}

static void test_insn_effect_add_rc()
{
    // add. r5, r3, r4 (Rc=1)
    InsnEffect fx = ppc_analyze_insn(ppc_add(5, 3, 4, true));
    CHECK("add. writes r5", fx.gpr_write & (1u << 5));
    CHECK("add. writes CR0", (fx.cr_write & (0xfu << 28)) == (0xfu << 28));
    CHECK("add. reads XER.SO for CR0", fx.xer_read & XER_BIT_SO);
}

static void test_insn_effect_cmpi()
{
    // cmpwi cr0, r3, 8
    InsnEffect fx = ppc_analyze_insn(ppc_cmpi(0, 3, 8));
    CHECK("cmpi reads r3", fx.gpr_read & (1u << 3));
    CHECK("cmpi writes CR0", (fx.cr_write & (0xfu << 28)) == (0xfu << 28));
    CHECK("cmpi reads XER.SO", fx.xer_read & XER_BIT_SO);
    CHECK("cmpi no GPR write", fx.gpr_write == 0);

    // cmpwi cr2, r5, 0
    InsnEffect fx2 = ppc_analyze_insn(ppc_cmpi(2, 5, 0));
    CHECK("cmpi cr2 reads r5", fx2.gpr_read & (1u << 5));
    CHECK("cmpi cr2 writes CR2", (fx2.cr_write & (0xfu << 20)) == (0xfu << 20));
    CHECK("cmpi cr2 doesn't write CR0", !(fx2.cr_write & (0xfu << 28)));
}

static void test_insn_effect_ori()
{
    // ori r3, r4, 0x1234
    InsnEffect fx = ppc_analyze_insn(ppc_ori(4, 3, 0x1234));
    CHECK("ori reads rS", fx.gpr_read & (1u << 4));
    CHECK("ori writes rA", fx.gpr_write & (1u << 3));
    CHECK("ori no CR", fx.cr_write == 0);
}

static void test_insn_effect_rlwinm()
{
    // rlwinm r3, r4, 2, 0, 29  (slwi r3, r4, 2)
    InsnEffect fx = ppc_analyze_insn(ppc_rlwinm(4, 3, 2, 0, 29));
    CHECK("rlwinm reads rS", fx.gpr_read & (1u << 4));
    CHECK("rlwinm writes rA", fx.gpr_write & (1u << 3));
    CHECK("rlwinm no CR (no Rc)", fx.cr_write == 0);

    // rlwinm. r3, r4, 2, 0, 29
    InsnEffect fx2 = ppc_analyze_insn(ppc_rlwinm(4, 3, 2, 0, 29, true));
    CHECK("rlwinm. writes CR0", (fx2.cr_write & (0xfu << 28)) == (0xfu << 28));
}

static void test_insn_effect_subfic()
{
    // subfic r3, r4, 10
    InsnEffect fx = ppc_analyze_insn(ppc_subfic(3, 4, 10));
    CHECK("subfic reads r4", fx.gpr_read & (1u << 4));
    CHECK("subfic writes r3", fx.gpr_write & (1u << 3));
    CHECK("subfic writes XER.CA", fx.xer_write & XER_BIT_CA);
}

static void test_insn_effect_srawi()
{
    // srawi r3, r4, 5
    InsnEffect fx = ppc_analyze_insn(ppc_srawi(4, 3, 5));
    CHECK("srawi reads rS", fx.gpr_read & (1u << 4));
    CHECK("srawi writes rA", fx.gpr_write & (1u << 3));
    CHECK("srawi writes XER.CA", fx.xer_write & XER_BIT_CA);
}

static void test_insn_effect_cr_logical()
{
    // crand cr0[lt], cr1[gt], cr2[eq]  (bit 0, bit 5, bit 10)
    InsnEffect fx = ppc_analyze_insn(ppc_crand(0, 5, 10));
    CHECK("crand reads crA bit", fx.cr_read & (1u << (31 - 5)));
    CHECK("crand reads crB bit", fx.cr_read & (1u << (31 - 10)));
    CHECK("crand writes crD bit", fx.cr_write & (1u << (31 - 0)));

    // crxor cr4, cr4, cr4  (crclr cr4)
    InsnEffect fx2 = ppc_analyze_insn(ppc_crxor(4, 4, 4));
    CHECK("crxor reads bit 4", fx2.cr_read & (1u << (31 - 4)));
    CHECK("crxor writes bit 4", fx2.cr_write & (1u << (31 - 4)));
}

static void test_insn_effect_unknown()
{
    // lwz r3, 0(r4) — not yet modeled, should be everything()
    uint32 lwz_opc = (32u << 26) | (3 << 21) | (4 << 16) | 0;
    InsnEffect fx = ppc_analyze_insn(lwz_opc);
    CHECK("unknown insn is_everything", fx.is_everything);
    CHECK("unknown reads all GPR", fx.gpr_read == 0xffffffff);
    CHECK("unknown writes all GPR", fx.gpr_write == 0xffffffff);
}

// ---- Liveness tests ----

static void test_liveness_dead_cr0()
{
    // add. r3, r4, r5   — writes r3 + CR0
    // ori  r3, r3, 0    — overwrites r3, never reads CR0
    // (block exit: everything live)
    //
    // CR0 from add. is dead: written but never read before block exit
    // ...wait, with conservative liveOut=everything, CR0 IS live at exit.
    // So let's test a case where CR0 is killed by a later write.

    // add. r3, r4, r5   — writes CR0
    // cmpi cr0, r3, 0   — writes CR0 (kills the first)
    // The first CR0 write is dead (overwritten by cmpi).
    uint32 insns[] = {
        ppc_add(3, 4, 5, true), // add. r3, r4, r5 — writes r3, CR0
        ppc_cmpi(0, 3, 0),      // cmpwi cr0, r3, 0 — writes CR0
    };
    InsnEffect effects[2];
    LivenessInfo liveness[2];
    for (int i = 0; i < 2; i++) {
        effects[i] = ppc_analyze_insn(insns[i]);
    }
    ppc_compute_liveness(effects, 2, liveness);

    CHECK("dead_cr0: add. CR0 is dead", (liveness[0].dead_cr & (0xfu << 28)) == (0xfu << 28));
    CHECK("dead_cr0: cmpi CR0 is live", (liveness[1].dead_cr & (0xfu << 28)) == 0);
}

static void test_liveness_dead_gpr()
{
    // li r3, 5       — writes r3
    // li r3, 10      — writes r3 (kills previous)
    // The first r3 write is dead.
    uint32 insns[] = {
        ppc_addi(3, 0, 5),  // li r3, 5
        ppc_addi(3, 0, 10), // li r3, 10
    };
    InsnEffect effects[2];
    LivenessInfo liveness[2];
    for (int i = 0; i < 2; i++) {
        effects[i] = ppc_analyze_insn(insns[i]);
    }
    ppc_compute_liveness(effects, 2, liveness);

    CHECK("dead_gpr: first li r3 is dead", liveness[0].dead_gpr & (1u << 3));
    CHECK("dead_gpr: second li r3 is live", !(liveness[1].dead_gpr & (1u << 3)));
}

static void test_liveness_read_keeps_alive()
{
    // li r3, 5       — writes r3
    // add r4, r3, r3 — reads r3, writes r4
    // li r3, 10      — writes r3
    // First r3 write is live (read by add).
    uint32 insns[] = {
        ppc_addi(3, 0, 5),  // li r3, 5
        ppc_add(4, 3, 3),   // add r4, r3, r3
        ppc_addi(3, 0, 10), // li r3, 10
    };
    InsnEffect effects[3];
    LivenessInfo liveness[3];
    for (int i = 0; i < 3; i++) {
        effects[i] = ppc_analyze_insn(insns[i]);
    }
    ppc_compute_liveness(effects, 3, liveness);

    CHECK("read_keeps_alive: first li r3 is live", !(liveness[0].dead_gpr & (1u << 3)));
    CHECK("read_keeps_alive: add r4 is live at exit", !(liveness[1].dead_gpr & (1u << 4)));
}

static void test_liveness_xer_ca()
{
    // subfic r3, r4, 10    — writes XER.CA
    // addic  r5, r3, 0     — writes XER.CA (kills previous)
    // First XER.CA write is dead.
    uint32 insns[] = {
        ppc_subfic(3, 4, 10),
        ppc_addic(5, 3, 0),
    };
    InsnEffect effects[2];
    LivenessInfo liveness[2];
    for (int i = 0; i < 2; i++) {
        effects[i] = ppc_analyze_insn(insns[i]);
    }
    ppc_compute_liveness(effects, 2, liveness);

    CHECK("xer_ca: first subfic CA is dead", liveness[0].dead_xer & XER_BIT_CA);
    CHECK("xer_ca: second addic CA is live", !(liveness[1].dead_xer & XER_BIT_CA));
}

static void test_liveness_cr_field_independence()
{
    // cmpi cr0, r3, 0    — writes CR0
    // cmpi cr2, r4, 0    — writes CR2 (doesn't kill CR0)
    // CR0 from first cmpi is live (not killed by CR2 write).
    uint32 insns[] = {
        ppc_cmpi(0, 3, 0),
        ppc_cmpi(2, 4, 0),
    };
    InsnEffect effects[2];
    LivenessInfo liveness[2];
    for (int i = 0; i < 2; i++) {
        effects[i] = ppc_analyze_insn(insns[i]);
    }
    ppc_compute_liveness(effects, 2, liveness);

    CHECK("cr_field_indep: CR0 from first cmpi is live", !(liveness[0].dead_cr & (0xfu << 28)));
    CHECK("cr_field_indep: CR2 from second cmpi is live", !(liveness[1].dead_cr & (0xfu << 20)));
}

// ---- ConcreteSemantics correctness tests ----

// Minimal CPU state for concrete testing
struct TestCPU {
    uint32 gpr[32];
    uint64 fpr[32];
    uint32 cr;
    uint32 fpscr;
    uint32 xer;
    uint32 xer_ca;
    uint32 lr;
    uint32 ctr;
    uint32 msr;
    uint32 pvr;
    uint32 pc;
    uint32 npc;
    uint32 current_opc;
};

static void test_concrete_addi()
{
    TestCPU cpu = {};
    cpu.gpr[4] = 100;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_addi(s, ppc_addi(3, 4, 5));
    CHECK("concrete addi: r3 = 105", cpu.gpr[3] == 105);

    // li r5, -1  (addi r5, 0, -1)
    ppc_sem_addi(s, ppc_addi(5, 0, (uint16_t)-1));
    CHECK("concrete li: r5 = 0xffffffff", cpu.gpr[5] == 0xffffffff);
}

static void test_concrete_add_rc()
{
    TestCPU cpu = {};
    cpu.gpr[3] = 10;
    cpu.gpr[4] = 20;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_addx(s, ppc_add(5, 3, 4, true));
    CHECK("concrete add.: r5 = 30", cpu.gpr[5] == 30);
    // CR0 should have GT set (result > 0)
    CHECK("concrete add.: CR0.GT", (cpu.cr >> 28) == 4);
}

static void test_concrete_add_rc_negative()
{
    TestCPU cpu = {};
    cpu.gpr[3] = 0xfffffffe; // -2
    cpu.gpr[4] = 1;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_addx(s, ppc_add(5, 3, 4, true));
    CHECK("concrete add. neg: r5 = -1", cpu.gpr[5] == 0xffffffff);
    CHECK("concrete add. neg: CR0.LT", (cpu.cr >> 28) == 8);
}

static void test_concrete_add_rc_zero()
{
    TestCPU cpu = {};
    cpu.gpr[3] = 0;
    cpu.gpr[4] = 0;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_addx(s, ppc_add(5, 3, 4, true));
    CHECK("concrete add. zero: r5 = 0", cpu.gpr[5] == 0);
    CHECK("concrete add. zero: CR0.EQ", (cpu.cr >> 28) == 2);
}

static void test_concrete_cmpi()
{
    TestCPU cpu = {};
    cpu.gpr[3] = 5;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_cmpi(s, ppc_cmpi(0, 3, 8));
    // 5 < 8 → LT
    CHECK("concrete cmpi LT: CR0=8", (cpu.cr >> 28) == 8);

    cpu.gpr[3] = 8;
    ppc_sem_cmpi(s, ppc_cmpi(0, 3, 8));
    CHECK("concrete cmpi EQ: CR0=2", (cpu.cr >> 28) == 2);

    cpu.gpr[3] = 10;
    ppc_sem_cmpi(s, ppc_cmpi(0, 3, 8));
    CHECK("concrete cmpi GT: CR0=4", (cpu.cr >> 28) == 4);
}

static void test_concrete_cmpi_cr2()
{
    TestCPU cpu = {};
    cpu.gpr[5] = 42;
    cpu.cr = 0;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_cmpi(s, ppc_cmpi(2, 5, 10));
    // 42 > 10 → GT in cr2
    uint32 cr2 = (cpu.cr >> 20) & 0xf;
    CHECK("concrete cmpi cr2: GT=4", cr2 == 4);
    // cr0 should be untouched
    CHECK("concrete cmpi cr2: cr0 untouched", (cpu.cr >> 28) == 0);
}

static void test_concrete_subfic()
{
    TestCPU cpu = {};
    cpu.gpr[4] = 3;

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_subfic(s, ppc_subfic(3, 4, 10));
    CHECK("concrete subfic: r3 = 7", cpu.gpr[3] == 7);
    CHECK("concrete subfic: CA set", cpu.xer & (1u << 29));
}

static void test_concrete_rlwinm()
{
    TestCPU cpu = {};
    cpu.gpr[4] = 0x12345678;

    ConcreteSemantics<TestCPU> s{cpu};
    // rlwinm r3, r4, 4, 0, 27  — rotate left 4 bits, mask upper 28 bits
    ppc_sem_rlwinmx(s, ppc_rlwinm(4, 3, 4, 0, 27));
    CHECK("concrete rlwinm: rotl4+mask", cpu.gpr[3] == 0x23456780);
}

static void test_concrete_srawi()
{
    TestCPU cpu = {};
    cpu.gpr[4] = 0xFFFFFFFC; // -4

    ConcreteSemantics<TestCPU> s{cpu};
    ppc_sem_srawix(s, ppc_srawi(4, 3, 1));
    CHECK("concrete srawi: r3 = -2", cpu.gpr[3] == 0xFFFFFFFE);
    // -4 >> 1 = -2, no bits shifted out that were 1... actually bit 0 of -4 is 0
    // -4 = 0xFFFFFFFC, bit 0 is 0, so CA should be 0
    CHECK("concrete srawi: CA clear", !(cpu.xer & (1u << 29)));

    cpu.gpr[4] = 0xFFFFFFFF; // -1
    ppc_sem_srawix(s, ppc_srawi(4, 3, 1));
    CHECK("concrete srawi -1>>1: r3 = -1", cpu.gpr[3] == 0xFFFFFFFF);
    CHECK("concrete srawi -1>>1: CA set", cpu.xer & (1u << 29));
}

static void test_concrete_cr_logical()
{
    TestCPU cpu = {};
    // Set CR bit 0 (cr0.LT), clear bit 1 (cr0.GT)
    cpu.cr = (1u << 31);

    ConcreteSemantics<TestCPU> s{cpu};
    // crand bit2, bit0, bit1  →  LT & GT = 0
    ppc_sem_crand(s, ppc_crand(2, 0, 1));
    CHECK("concrete crand: 1&0 = 0", !(cpu.cr & (1u << (31 - 2))));

    // crxor bit3, bit0, bit0  →  LT ^ LT = 0
    ppc_sem_crxor(s, ppc_crxor(3, 0, 0));
    CHECK("concrete crxor: 1^1 = 0", !(cpu.cr & (1u << (31 - 3))));
}

// ---- Main ----

int main()
{
    printf("=== InsnEffect tests ===\n");
    test_insn_effect_addi();
    test_insn_effect_addi_r0();
    test_insn_effect_add();
    test_insn_effect_add_rc();
    test_insn_effect_cmpi();
    test_insn_effect_ori();
    test_insn_effect_rlwinm();
    test_insn_effect_subfic();
    test_insn_effect_srawi();
    test_insn_effect_cr_logical();
    test_insn_effect_unknown();

    printf("=== Liveness tests ===\n");
    test_liveness_dead_cr0();
    test_liveness_dead_gpr();
    test_liveness_read_keeps_alive();
    test_liveness_xer_ca();
    test_liveness_cr_field_independence();

    printf("=== ConcreteSemantics tests ===\n");
    test_concrete_addi();
    test_concrete_add_rc();
    test_concrete_add_rc_negative();
    test_concrete_add_rc_zero();
    test_concrete_cmpi();
    test_concrete_cmpi_cr2();
    test_concrete_subfic();
    test_concrete_rlwinm();
    test_concrete_srawi();
    test_concrete_cr_logical();

    printf("\n=== %d tests, %d failures ===\n", tests, failures);
    return failures;
}
