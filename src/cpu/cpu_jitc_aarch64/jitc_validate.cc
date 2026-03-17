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
#include "io/io.h"

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

// Copy PPC-visible registers from src to dst.
// Does NOT copy: jitc pointer, JIT internals (pc_ofs, current_code_base,
// exception flags, TLB cache, translation state).
static void syncCPUState(PPC_CPU_State *dst, const PPC_CPU_State *src)
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
	refCPU = (PPC_CPU_State *)malloc(sizeof(PPC_CPU_State));
	memset(refCPU, 0, sizeof(PPC_CPU_State));
	syncCPUState(refCPU, gCPU);
	refCPU->jitc = NULL;
	refMemory = gMemory;
	gValidateRefMemory = NULL;

	gValidateMode = true;
	valLog = fopen("validate.log", "w");
	fprintf(stderr, "[VALIDATE] initialized, memory=%uMB\n", gMemorySize / (1024 * 1024));
}

static void refStepOne()
{
	gIOReadReplay = true;

	uint32 physAddr;
	int r = ppc_effective_to_physical(*refCPU, refCPU->pc, PPC_MMU_READ | PPC_MMU_CODE, physAddr);
	if (r != PPC_MMU_OK) {
		gIOReadReplay = false;
		return;
	}

	uint32 opc;
	ppc_read_physical_word(physAddr, opc);

	refCPU->npc = refCPU->pc + 4;
	refCPU->current_opc = opc;

	if (opc == PROM_MAGIC_OPCODE) {
		refCPU->pc = refCPU->npc;
		gIOReadReplay = false;
		return;
	}

	ppc_exec_opc(*refCPU);
	refCPU->pc = refCPU->npc;

	refCPU->ptb++;

	gIOReadReplay = false;
	gIOReadCacheValid = false;
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
	// SRR0/SRR1 and DEC are set by the JIT's asynchronous timer.
	// The reference doesn't have its own timer — sync from JIT.
	refCPU->srr[0] = gCPU->srr[0];
	refCPU->srr[1] = gCPU->srr[1];
	refCPU->dec = gCPU->dec;
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
		// Full resync — copy only PPC-visible registers
		syncCPUState(refCPU, gCPU);
		refCPU->pc = effectivePC;
		return;
	}

	// PCs match. Compare all registers.
	static uint32 prevPC = 0;
	bool needStep = true;
	static int loopCount = 0;
	if (effectivePC == 0xc0205128) {
		loopCount++;
		if (loopCount == 1 || loopCount % 1000000 == 0)
			fprintf(stderr, "[LOOP] #%d msr=%08x ee=%d dec=%08x exc_pend=%d dec_exc=%d ext_exc=%d\n",
				loopCount, gCPU->msr, (gCPU->msr >> 15) & 1, gCPU->dec,
				gCPU->exception_pending, gCPU->dec_exception, gCPU->ext_exception);
	}
	// Log around kernel entry
	if (effectivePC >= 0xc0003000 && effectivePC <= 0xc0003020 && valLog) {
		uint32 dbg_pa = 0, dbg_insn = 0;
		ppc_effective_to_physical(*gCPU, effectivePC, PPC_MMU_READ | PPC_MMU_CODE | 16, dbg_pa);
		ppc_read_physical_word(dbg_pa, dbg_insn);
		fprintf(valLog, "KERNEL pc=%08x (pa=%08x insn=%08x) r31: jit=%08x ref=%08x opc: jit=%08x ref=%08x\n",
			effectivePC, dbg_pa, dbg_insn, gCPU->gpr[31], refCPU->gpr[31],
			gCPU->current_opc, refCPU->current_opc);
	}
	if (!compareStates()) {
		bool isProm = (prevPC == gPromOSIEntry || prevPC == gPromOSIEntry + 4);
		// Check if previous instruction was an I/O read (volatile register).
		// Detect by checking if the previous instruction's opcode is a load
		// and the effective address is in I/O space (>= gMemorySize).
		bool isIO = false;
		if (!isProm) {
			// Check if previous instruction was a load from I/O space.
			// Decode the previous instruction to get the load EA.
			uint32 prevInsn = 0;
			uint32 prevPA = 0;
			if (ppc_effective_to_physical(*gCPU, prevPC, PPC_MMU_READ | PPC_MMU_CODE | 16, prevPA) == 0) {
				ppc_read_physical_word(prevPA, prevInsn);
			}
			uint32 opc = (prevInsn >> 26) & 0x3f;
			int rA_field = (prevInsn >> 16) & 0x1f;
			int16_t d_field = (int16_t)(prevInsn & 0xffff);
			uint32 loadEA = 0;
			// D-form loads: lbz(34), lhz(40), lha(42), lwz(32)
			if (opc == 34 || opc == 40 || opc == 42 || opc == 32) {
				loadEA = (rA_field ? gCPU->gpr[rA_field] : 0) + d_field;
			}
			// X-form (opcode 31): loads use rA + rB, also mftb/mfspr
			if (opc == 31) {
				uint32 xo = (prevInsn >> 1) & 0x3ff;
				if (xo == 371 || xo == 339) {
					// mftb or mfspr — volatile, always resync
					isIO = true;
				} else {
					int rB_field = (prevInsn >> 11) & 0x1f;
					loadEA = (rA_field ? gCPU->gpr[rA_field] : 0) + gCPU->gpr[rB_field];
				}
			}
			// Check if loadEA translates to I/O space (PA >= gMemorySize)
			if (loadEA && !isIO) {
				uint32 loadPA = 0;
				if (ppc_effective_to_physical(*gCPU, loadEA, PPC_MMU_READ | 16, loadPA) == 0) {
					if (loadPA >= gMemorySize) {
						isIO = true;
					}
				}
				// Also try with the reference's EA (rA may have been overwritten by load)
				if (!isIO) {
					uint32 refEA = 0;
					if (opc == 34 || opc == 40 || opc == 42 || opc == 32) {
						refEA = (rA_field ? refCPU->gpr[rA_field] : 0) + d_field;
					}
					if (refEA) {
						uint32 refPA = 0;
						if (ppc_effective_to_physical(*gCPU, refEA, PPC_MMU_READ | 16, refPA) == 0) {
							if (refPA >= gMemorySize) {
								isIO = true;
							}
						}
					}
				}
			}
			// If the previous instruction was a load and the destination
			// register has 0 in the reference (our I/O skip returns 0),
			// treat it as an I/O mismatch.
			if (!isIO && (opc == 32 || opc == 33 || opc == 34 || opc == 35 ||
			              opc == 40 || opc == 41 || opc == 42 || opc == 43)) {
				int rD_field = (prevInsn >> 21) & 0x1f;
				if (refCPU->gpr[rD_field] == 0 && gCPU->gpr[rD_field] != 0) {
					isIO = true;
				}
			}
		}
		if (isProm || isIO) {
			// Resync all registers — I/O and volatile SPR reads can
			// write to any register (including callee-saved like r29).
			syncCPUState(refCPU, gCPU);
			refCPU->pc = effectivePC;
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
				fprintf(valLog, "  r1(sp)=%08x r9=%08x\n", gCPU->gpr[1], gCPU->gpr[9]);
				fprintf(valLog, "  jit: msr=%08x srr0=%08x srr1=%08x opc=%08x\n",
					gCPU->msr, gCPU->srr[0], gCPU->srr[1], gCPU->current_opc);
				fprintf(valLog, "  ref: msr=%08x srr0=%08x srr1=%08x opc=%08x\n",
					refCPU->msr, refCPU->srr[0], refCPU->srr[1], refCPU->current_opc);
				uint32 dbg_pa = 0, dbg_insn = 0;
				if (ppc_effective_to_physical(*gCPU, effectivePC, PPC_MMU_READ | PPC_MMU_CODE | 16, dbg_pa) == 0) {
					ppc_read_physical_word(dbg_pa, dbg_insn);
					fprintf(valLog, "  insn@%08x (pa=%08x): %08x\n", effectivePC, dbg_pa, dbg_insn);
				} else {
					fprintf(valLog, "  insn@%08x: XLAT FAILED\n", effectivePC);
				}
				// Also check prev instruction
				uint32 dbg_pa2 = 0, dbg_insn2 = 0;
				if (ppc_effective_to_physical(*gCPU, prevPC, PPC_MMU_READ | PPC_MMU_CODE | 16, dbg_pa2) == 0) {
					ppc_read_physical_word(dbg_pa2, dbg_insn2);
					fprintf(valLog, "  insn@%08x (pa=%08x): %08x\n", prevPC, dbg_pa2, dbg_insn2);
				}
				// Also check ref's translation of same address
				uint32 ref_pa = 0;
				if (ppc_effective_to_physical(*refCPU, effectivePC, PPC_MMU_READ | PPC_MMU_CODE | 16, ref_pa) == 0) {
					uint32 ref_insn = 0;
					ppc_read_physical_word(ref_pa, ref_insn);
					fprintf(valLog, "  ref insn@%08x (pa=%08x): %08x\n", effectivePC, ref_pa, ref_insn);
				}
			}
			diverged = true;
			if (valLog) fflush(valLog);
			fprintf(stderr, "[VALIDATE] MISMATCH #%llu pc=%08x. See validate.log\n", valCount, effectivePC);
			exit(1);
		}
	}

	if (needStep) {
		// Check if current instruction is an I/O access.
		// If so, skip reference step to avoid double I/O side effects
		// (both JIT and reference read/write the same I/O devices).
		refStepOne();
	}

	prevPC = effectivePC;
	if (valCount % 10000 == 0) {
		fprintf(stderr, "[VALIDATE] %llu instructions OK (pc=%08x)\n", valCount, effectivePC);
	}
}
