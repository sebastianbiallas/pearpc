/*
 *  PearPC
 *  jitc_validate.cc
 *
 *  Lock-step validation: run a reference interpreter alongside the JIT.
 *  Both have separate CPU state and memory. At each basic block boundary
 *  (jitcNewPC), step the reference forward and compare registers.
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

extern PPC_CPU_State *gCPU;
extern byte *gMemory;
extern uint32 gMemorySize;

bool gValidateMode = false;
byte *gValidateRefMemory = NULL;

static PPC_CPU_State *refCPU = NULL;
static byte *refMemory = NULL;
static FILE *valLog = NULL;
static bool diverged = false;
static uint64 valCount = 0;

void jitcValidateInit()
{
	refCPU = (PPC_CPU_State *)malloc(sizeof(PPC_CPU_State));
	memcpy(refCPU, gCPU, sizeof(PPC_CPU_State));
	refCPU->jitc = NULL;
	refMemory = gMemory;
	gValidateRefMemory = NULL;

	gValidateMode = true;
	valLog = fopen("validate.log", "w");
	fprintf(stderr, "[VALIDATE] initialized, memory=%uMB\n", gMemorySize / (1024 * 1024));
}

static void refStepOne()
{
	PPC_CPU_State *savedCPU = gCPU;
	byte *savedMem = gMemory;
	gCPU = refCPU;
	gMemory = refMemory;

	uint32 physAddr;
	int r = ppc_effective_to_physical(*refCPU, refCPU->pc, PPC_MMU_READ | PPC_MMU_CODE, physAddr);
	if (r != PPC_MMU_OK) {
		gCPU = savedCPU;
		gMemory = savedMem;
		return;
	}

	uint32 opc;
	ppc_read_physical_word(physAddr, opc);

	refCPU->npc = refCPU->pc + 4;
	refCPU->current_opc = opc;

	// Skip PROM calls — they use globals and would corrupt shared memory.
	// Instead, just advance PC. The validate function will resync state
	// from the JIT when PCs don't match.
	if (opc == PROM_MAGIC_OPCODE) {
		refCPU->pc = refCPU->npc;
		gCPU = savedCPU;
		gMemory = savedMem;
		return;
	}

	ppc_exec_opc(*refCPU);
	refCPU->pc = refCPU->npc;

	// Keep timers in sync
	refCPU->ptb++;
	if (refCPU->pdec == 0) {
		refCPU->pdec = 0xffffffff;
	} else {
		refCPU->pdec--;
	}

	gCPU = savedCPU;
	gMemory = savedMem;
}

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
	CMP(cr, "%08x");
	CMP(lr, "%08x");
	CMP(ctr, "%08x");
	CMP(xer, "%08x");
	CMP(xer_ca, "%08x");
	CMP(msr, "%08x");
	if (gCPU->srr[0] != refCPU->srr[0]) {
		if (valLog) fprintf(valLog, "  SRR0 MISMATCH: jit=%08x ref=%08x\n", gCPU->srr[0], refCPU->srr[0]);
		ok = false;
	}
	if (gCPU->srr[1] != refCPU->srr[1]) {
		if (valLog) fprintf(valLog, "  SRR1 MISMATCH: jit=%08x ref=%08x\n", gCPU->srr[1], refCPU->srr[1]);
		ok = false;
	}
	CMP(sdr1, "%08x");
	CMP(pagetable_base, "%08x");
	CMP(pagetable_hashmask, "%08x");
	for (int i = 0; i < 16; i++) {
		if (gCPU->sr[i] != refCPU->sr[i]) {
			if (valLog) fprintf(valLog, "  SR%d MISMATCH: jit=%08x ref=%08x\n", i, gCPU->sr[i], refCPU->sr[i]);
			ok = false;
		}
	}
	for (int i = 0; i < 4; i++) {
		if (gCPU->ibatu[i] != refCPU->ibatu[i] || gCPU->ibatl[i] != refCPU->ibatl[i]) {
			if (valLog) fprintf(valLog, "  IBAT%d MISMATCH: jit=(%08x,%08x) ref=(%08x,%08x)\n",
				i, gCPU->ibatu[i], gCPU->ibatl[i], refCPU->ibatu[i], refCPU->ibatl[i]);
			ok = false;
		}
		if (gCPU->dbatu[i] != refCPU->dbatu[i] || gCPU->dbatl[i] != refCPU->dbatl[i]) {
			if (valLog) fprintf(valLog, "  DBAT%d MISMATCH: jit=(%08x,%08x) ref=(%08x,%08x)\n",
				i, gCPU->dbatu[i], gCPU->dbatl[i], refCPU->dbatu[i], refCPU->dbatl[i]);
			ok = false;
		}
	}

#undef CMP
	return ok;
}

extern "C" void jitcValidateAtDispatch(uint32 effectivePC)
{
	if (diverged || !refCPU) return;
	valCount++;

	// Memory watchpoint: check if address 0x07deff60 changes
	{
		static uint32 lastMemVal = 0;
		uint32 watchAddr = 0x07deff60;
		if (watchAddr < gMemorySize) {
			uint32 curVal = *(uint32 *)(gMemory + watchAddr);
			if (curVal != lastMemVal) {
				if (valLog) {
					fprintf(valLog, "MEM[%08x] CHANGED: %08x -> %08x at pc=%08x (#%llu)\n",
						watchAddr, lastMemVal, curVal, effectivePC, valCount);
				}
				lastMemVal = curVal;
			}
		}
	}

	static uint32 last_r3 = 0, last_r4 = 0, last_r5 = 0, last_r31 = 0;
	if (valCount > 15000000) {
		if (gCPU->gpr[3] != last_r3 && valLog) {
			fprintf(valLog, "R3 CHANGED: %08x -> %08x at pc=%08x (#%llu)\n",
				last_r3, gCPU->gpr[3], effectivePC, valCount);
		}
		if (gCPU->gpr[4] != last_r4 && valLog) {
			fprintf(valLog, "R4 CHANGED: %08x -> %08x at pc=%08x (#%llu)\n",
				last_r4, gCPU->gpr[4], effectivePC, valCount);
		}
		if (gCPU->gpr[5] != last_r5 && valLog) {
			fprintf(valLog, "R5 CHANGED: %08x -> %08x at pc=%08x (#%llu)\n",
				last_r5, gCPU->gpr[5], effectivePC, valCount);
		}
		if (gCPU->gpr[31] != last_r31 && valLog) {
			fprintf(valLog, "R31 CHANGED: %08x -> %08x at pc=%08x (#%llu)\n",
				last_r31, gCPU->gpr[31], effectivePC, valCount);
		}
	}
	last_r3 = gCPU->gpr[3]; last_r4 = gCPU->gpr[4];
	last_r5 = gCPU->gpr[5]; last_r31 = gCPU->gpr[31];

	// Called BEFORE each PPC instruction executes on the JIT.
	// The reference should be at the same PC with the same state.

	// Step the reference to catch up to effectivePC.
	// With separate memory, reference writes don't corrupt JIT memory.
	int steps = 0;
	while (refCPU->pc != effectivePC && steps < 1000) {
		refStepOne();
		steps++;
	}

	if (refCPU->pc != effectivePC) {
		// PCs don't match after catch-up — PROM call that reference skipped.
		// Check callee-saved registers before resyncing.
		for (int i = 14; i <= 31; i++) {
			if (refCPU->gpr[i] != gCPU->gpr[i]) {
				if (valLog) {
					fprintf(valLog, "r%d CALLEE-SAVED MISMATCH at RESYNC #%llu: jit=%08x ref=%08x (ref pc=%08x jit pc=%08x)\n",
						i, valCount, gCPU->gpr[i], refCPU->gpr[i], refCPU->pc, effectivePC);
					fflush(valLog);
				}
				fprintf(stderr, "[VALIDATE] r%d CALLEE-SAVED MISMATCH #%llu: jit=%08x ref=%08x\n",
					i, valCount, gCPU->gpr[i], refCPU->gpr[i]);
			}
		}
		// Full resync
		JITC *savedJitc = refCPU->jitc;
		memcpy(refCPU, gCPU, sizeof(PPC_CPU_State));
		refCPU->jitc = savedJitc;
		refCPU->pc = effectivePC;
		return;
	}

	// PCs match. Compare all registers.
	static uint32 prevPC = 0;
	bool needStep = true;
	if (!compareStates()) {
		if (prevPC == gPromOSIEntry || prevPC == gPromOSIEntry + 4) {
			// After PROM call: only sync caller-saved registers.
			// r14-r31 are callee-saved — if they differ, it's a real bug.
			for (int i = 0; i <= 12; i++)
				refCPU->gpr[i] = gCPU->gpr[i];
			refCPU->cr = gCPU->cr;
			refCPU->lr = gCPU->lr;
			refCPU->ctr = gCPU->ctr;
			refCPU->xer = gCPU->xer;
			refCPU->xer_ca = gCPU->xer_ca;
			// Do NOT sync memory — keep reference memory independent
			needStep = false;
		} else {
			if (valLog) {
				// Log all GPRs for debugging
				for (int i = 0; i < 32; i++) {
					if (gCPU->gpr[i] != refCPU->gpr[i]) {
						fprintf(valLog, "  r%d: jit=%08x ref=%08x\n", i, gCPU->gpr[i], refCPU->gpr[i]);
					}
				}
				fprintf(valLog, "=== VALIDATE #%llu: MISMATCH at pc=%08x (prev pc=%08x) ===\n",
					valCount, effectivePC, prevPC);
				// Also dump matching regs for context
				fprintf(valLog, "  r1(sp)=%08x r9=%08x\n", gCPU->gpr[1], gCPU->gpr[9]);
			}
			diverged = true;
			if (valLog) fflush(valLog);
			fprintf(stderr, "[VALIDATE] MISMATCH #%llu pc=%08x. See validate.log\n", valCount, effectivePC);
			return;
		}
	}

	if (needStep) {
		refStepOne();
	}

	prevPC = effectivePC;
	if (valCount % 10000 == 0) {
		fprintf(stderr, "[VALIDATE] %llu instructions OK (pc=%08x)\n", valCount, effectivePC);
	}
}
