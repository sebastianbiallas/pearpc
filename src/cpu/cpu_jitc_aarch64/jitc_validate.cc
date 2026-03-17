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

extern PPC_CPU_State *gCPU;
extern byte *gMemory;
extern uint32 gMemorySize;

bool gValidateMode = false;

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

	// Share the same memory — both execute the same instructions,
	// so writes should be identical. This avoids 128MB memcpy.
	refMemory = gMemory;

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

#undef CMP
	return ok;
}

extern "C" void jitcValidateAtDispatch(uint32 effectivePC)
{
	if (diverged || !refCPU) return;
	valCount++;

	// Called BEFORE each PPC instruction executes on the JIT.
	// The reference should be at the same PC with the same state.

	// First: step the reference to catch up to effectivePC
	// (needed because some JIT dispatches skip the validate call,
	// e.g. ppc_new_pc_asm → direct jump)
	int steps = 0;
	while (refCPU->pc != effectivePC && steps < 200) {
		refStepOne();
		steps++;
	}

	if (refCPU->pc != effectivePC) {
		if (valLog) {
			fprintf(valLog, "=== VALIDATE #%llu: DIVERGED ref=%08x jit=%08x (after %d steps) ===\n",
				valCount, refCPU->pc, effectivePC, steps);
		}
		diverged = true;
		if (valLog) fflush(valLog);
		fprintf(stderr, "[VALIDATE] DIVERGED #%llu ref=%08x jit=%08x\n", valCount, refCPU->pc, effectivePC);
		return;
	}

	// PCs match. Compare all registers.
	static uint32 prevPC = 0;
	if (!compareStates()) {
		if (valLog) {
			fprintf(valLog, "=== VALIDATE #%llu: MISMATCH at pc=%08x (prev pc=%08x) ===\n",
				valCount, effectivePC, prevPC);
		}
		diverged = true;
		if (valLog) fflush(valLog);
		fprintf(stderr, "[VALIDATE] MISMATCH #%llu pc=%08x. See validate.log\n", valCount, effectivePC);
		return;
	}

	// Step reference one instruction (so it's ready for the next compare)
	refStepOne();

	prevPC = effectivePC;
	if (valCount % 10000 == 0) {
		fprintf(stderr, "[VALIDATE] %llu instructions OK (pc=%08x)\n", valCount, effectivePC);
	}
}
