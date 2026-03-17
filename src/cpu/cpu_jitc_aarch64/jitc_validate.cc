/*
 *  PearPC
 *  jitc_validate.cc
 *
 *  Lock-step validation: compare JIT output with reference interpreter
 *  after each instruction.
 *
 *  Design:
 *  - The JIT is the master. It handles all non-determinism (PROM, I/O, interrupts).
 *  - The reference only executes normal CPU instructions.
 *  - Before each instruction: copy JIT state → reference, execute on both, compare.
 *  - If PCs don't match (PROM call, interrupt): just sync from JIT, no error.
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "system/arch/sysendian.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_mmu.h"
#include "io/prom/promosi.h"
#include "io/io.h"

extern PPC_CPU_State *gCPU;
extern byte *gMemory;
extern uint32 gMemorySize;

bool gValidateMode = false;
byte *gValidateRefMemory = NULL;

static PPC_CPU_State refCPU_storage;
static PPC_CPU_State *refCPU = &refCPU_storage;
static FILE *valLog = NULL;
static uint64 valCount = 0;

// Copy all PPC-visible registers from src to dst.
static void syncState(PPC_CPU_State *dst, const PPC_CPU_State *src)
{
	for (int i = 0; i < 32; i++) dst->gpr[i] = src->gpr[i];
	for (int i = 0; i < 32; i++) dst->fpr[i] = src->fpr[i];
	dst->cr = src->cr;
	dst->fpscr = src->fpscr;
	dst->xer = src->xer;
	dst->xer_ca = src->xer_ca;
	dst->lr = src->lr;
	dst->ctr = src->ctr;
	dst->msr = src->msr;
	dst->pvr = src->pvr;
	for (int i = 0; i < 4; i++) {
		dst->ibatu[i] = src->ibatu[i];
		dst->ibatl[i] = src->ibatl[i];
		dst->ibat_bl[i] = src->ibat_bl[i];
		dst->ibat_nbl[i] = src->ibat_nbl[i];
		dst->ibat_bepi[i] = src->ibat_bepi[i];
		dst->ibat_brpn[i] = src->ibat_brpn[i];
		dst->dbatu[i] = src->dbatu[i];
		dst->dbatl[i] = src->dbatl[i];
		dst->dbat_bl[i] = src->dbat_bl[i];
		dst->dbat_nbl[i] = src->dbat_nbl[i];
		dst->dbat_bepi[i] = src->dbat_bepi[i];
		dst->dbat_brpn[i] = src->dbat_brpn[i];
	}
	dst->sdr1 = src->sdr1;
	for (int i = 0; i < 16; i++) dst->sr[i] = src->sr[i];
	dst->dar = src->dar;
	dst->dsisr = src->dsisr;
	for (int i = 0; i < 4; i++) dst->sprg[i] = src->sprg[i];
	dst->srr[0] = src->srr[0];
	dst->srr[1] = src->srr[1];
	dst->dec = src->dec;
	dst->ear = src->ear;
	dst->pir = src->pir;
	dst->tb = src->tb;
	for (int i = 0; i < 16; i++) dst->hid[i] = src->hid[i];
	dst->pagetable_base = src->pagetable_base;
	dst->pagetable_hashmask = src->pagetable_hashmask;
	dst->reserve = src->reserve;
	dst->have_reservation = src->have_reservation;
}

void jitcValidateInit()
{
	memset(refCPU, 0, sizeof(PPC_CPU_State));
	syncState(refCPU, gCPU);
	refCPU->jitc = NULL;

	gValidateMode = true;
	gValidateRefMemory = NULL;
	valLog = fopen("validate.log", "w");
	fprintf(stderr, "[VALIDATE] initialized, memory=%uMB\n", gMemorySize / (1024 * 1024));
}

// Compare JIT and reference state. Return true if match.
static bool compareStates()
{
	bool ok = true;

#define CMP(field, fmt) \
	if (gCPU->field != refCPU->field) { \
		if (valLog) fprintf(valLog, "  " #field " MISMATCH: jit=" fmt " ref=" fmt "\n", gCPU->field, refCPU->field); \
		ok = false; \
	}

	for (int i = 0; i < 32; i++) {
		if (gCPU->gpr[i] != refCPU->gpr[i]) {
			if (valLog) fprintf(valLog, "  r%d MISMATCH: jit=%08x ref=%08x\n", i, gCPU->gpr[i], refCPU->gpr[i]);
			ok = false;
		}
	}
	for (int i = 0; i < 32; i++) {
		if (gCPU->fpr[i] != refCPU->fpr[i]) {
			if (valLog) fprintf(valLog, "  f%d MISMATCH\n", i);
			ok = false;
		}
	}
	CMP(cr, "%08x");
	CMP(lr, "%08x");
	CMP(ctr, "%08x");
	CMP(xer, "%08x");
	CMP(xer_ca, "%08x");
	CMP(msr, "%08x");
	CMP(srr[0], "%08x");
	CMP(srr[1], "%08x");
	CMP(sdr1, "%08x");
	for (int i = 0; i < 16; i++) {
		if (gCPU->sr[i] != refCPU->sr[i]) {
			if (valLog) fprintf(valLog, "  SR%d MISMATCH: jit=%08x ref=%08x\n", i, gCPU->sr[i], refCPU->sr[i]);
			ok = false;
		}
	}
	for (int i = 0; i < 4; i++) {
		if (gCPU->ibatu[i] != refCPU->ibatu[i] || gCPU->ibatl[i] != refCPU->ibatl[i]) {
			if (valLog) fprintf(valLog, "  IBAT%d MISMATCH\n", i);
			ok = false;
		}
		if (gCPU->dbatu[i] != refCPU->dbatu[i] || gCPU->dbatl[i] != refCPU->dbatl[i]) {
			if (valLog) fprintf(valLog, "  DBAT%d MISMATCH\n", i);
			ok = false;
		}
	}

#undef CMP
	return ok;
}

/*
 * Called after each JIT instruction (at each dispatch point).
 * effectivePC = the PC the JIT is about to execute.
 *
 * Protocol:
 *   1. If refCPU->pc != effectivePC: non-deterministic event happened
 *      (PROM call, interrupt, I/O). Sync from JIT. No comparison.
 *   2. If refCPU->pc == effectivePC: compare states. If mismatch, error.
 *      Then step the reference one instruction. If the instruction is
 *      non-deterministic (PROM, I/O), sync from JIT instead of stepping.
 */
extern "C" void jitcValidateAtDispatch(uint32 effectivePC)
{
	if (!refCPU) return;
	valCount++;

	if (refCPU->pc != effectivePC) {
		// Non-deterministic event: PROM call, interrupt, I/O.
		// JIT is the source of truth. Sync everything.
		syncState(refCPU, gCPU);
		refCPU->pc = effectivePC;
		goto step;
	}

	// PCs match. Compare all registers.
	if (!compareStates()) {
		if (valLog) {
			fprintf(valLog, "=== VALIDATE #%llu: MISMATCH at pc=%08x ===\n",
				valCount, effectivePC);
			uint32 pa = 0, insn = 0;
			if (ppc_effective_to_physical(*gCPU, effectivePC, PPC_MMU_READ | PPC_MMU_CODE | 16, pa) == 0)
				ppc_read_physical_word(pa, insn);
			fprintf(valLog, "  insn@%08x (pa=%08x): %08x\n", effectivePC, pa, insn);
			fflush(valLog);
		}
		fprintf(stderr, "[VALIDATE] MISMATCH #%llu pc=%08x. See validate.log\n", valCount, effectivePC);
		exit(1);
	}

step:
	// Step the reference one instruction.
	{
		uint32 physAddr;
		int r = ppc_effective_to_physical(*refCPU, refCPU->pc, PPC_MMU_READ | PPC_MMU_CODE, physAddr);
		if (r != PPC_MMU_OK) {
			// Can't translate — ISI. JIT will handle it. Sync next time.
			return;
		}

		uint32 opc;
		ppc_read_physical_word(physAddr, opc);
		refCPU->current_opc = opc;
		refCPU->npc = refCPU->pc + 4;

		// Non-deterministic instructions: don't execute on reference.
		// JIT handles them. Sync will happen on next call when PCs differ.
		if (opc == PROM_MAGIC_OPCODE) {
			// PROM call. Don't advance PC — keep reference at the
			// PROM opcode. Next dispatch: PCs won't match → sync from JIT.
			return;
		}

		// Normal instruction: execute on reference.
		gIOReadReplay = true;
		ppc_exec_opc(*refCPU);
		gIOReadReplay = false;
		gIOReadCacheValid = false;

		refCPU->pc = refCPU->npc;
	}

	if (valCount % 10000 == 0) {
		fprintf(stderr, "[VALIDATE] %llu instructions OK (pc=%08x)\n", valCount, effectivePC);
	}
}
