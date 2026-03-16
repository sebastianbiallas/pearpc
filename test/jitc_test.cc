/*
 *  PearPC AArch64 JIT Test Harness
 *
 *  Loads raw PPC machine code into emulated memory and runs it
 *  through the JIT infrastructure. No PROM, no devices, no config.
 *
 *  Usage: jitc_test [ppc_binary_file]
 *
 *  If no file given, runs a built-in test program.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpu/cpu.h"
#include "system/types.h"
#include "tools/snprintf.h"

/*
 *  We need the JITC-specific CPU state and entry point.
 *  These are defined in the cpu_jitc_aarch64 backend.
 */
extern bool ppc_cpu_init();
extern void ppc_cpu_init_config();
extern bool ppc_init_physical_memory(uint size);
extern uint32 ppc_get_memory_size();
extern byte *gMemory;
extern uint32 gMemorySize;

/*
 *  Minimal built-in PPC test program.
 *  Assembled by hand - each instruction is 4 bytes, big-endian.
 *
 *  Address 0x1000:
 *    li      r3, 0           # 38 60 00 00
 *    li      r4, 10          # 38 80 00 0A
 *  loop:
 *    addi    r3, r3, 1       # 38 63 00 01
 *    cmpwi   r3, 10          # 2C 03 00 0A
 *    blt     loop            # 41 80 FF F8  (branch back 2 instructions)
 *    li      r5, 0x42        # 38 A0 00 42  (marker: r5 = 0x42)
 *
 *  After execution, r3 should be 10, r5 should be 0x42.
 */
static const uint32 test_program[] = {
    0x38600000,  // li r3, 0
    0x3880000A,  // li r4, 10
    0x38630001,  // addi r3, r3, 1
    0x2C03000A,  // cmpwi r3, 10
    0x4180FFF8,  // blt -8 (back to addi)
    0x38A00042,  // li r5, 0x42
    0x00000000,  // illegal - will trap
};

#define TEST_LOAD_ADDR 0x1000
#define MEMORY_SIZE (4 * 1024 * 1024)  // 4 MB, minimal

/*
 *  Write a big-endian 32-bit word to emulated memory
 */
static void write_be32(byte *mem, uint32 addr, uint32 val)
{
    mem[addr + 0] = (val >> 24) & 0xFF;
    mem[addr + 1] = (val >> 16) & 0xFF;
    mem[addr + 2] = (val >>  8) & 0xFF;
    mem[addr + 3] = (val >>  0) & 0xFF;
}

static uint32 read_be32(byte *mem, uint32 addr)
{
    return ((uint32)mem[addr + 0] << 24) |
           ((uint32)mem[addr + 1] << 16) |
           ((uint32)mem[addr + 2] <<  8) |
           ((uint32)mem[addr + 3] <<  0);
}

int main(int argc, char *argv[])
{
    printf("=== PearPC AArch64 JIT Test Harness ===\n\n");

    // Allocate physical memory
    if (!ppc_init_physical_memory(MEMORY_SIZE)) {
        printf("FATAL: Cannot allocate %d bytes of memory\n", MEMORY_SIZE);
        return 1;
    }
    printf("Memory: %d bytes at %p\n", gMemorySize, gMemory);

    // Load test program into memory
    const uint32 *program = test_program;
    int program_words = sizeof(test_program) / sizeof(test_program[0]);
    const char *program_source = "built-in test";

    // TODO: if argv[1] given, load a raw PPC binary from file instead

    printf("Loading %s (%d instructions) at 0x%x\n",
           program_source, program_words, TEST_LOAD_ADDR);

    for (int i = 0; i < program_words; i++) {
        write_be32(gMemory, TEST_LOAD_ADDR + i * 4, program[i]);
    }

    // Verify the load
    printf("Verify: first instruction = 0x%08x (expect 0x%08x)\n",
           read_be32(gMemory, TEST_LOAD_ADDR), test_program[0]);

    // Initialize CPU
    if (!ppc_cpu_init()) {
        printf("FATAL: ppc_cpu_init failed\n");
        return 1;
    }

    // Set initial PC
    ppc_cpu_set_pc(0, TEST_LOAD_ADDR);

    // Set MSR: no translation (real mode), FP enabled
    // MSR_FP = 0x2000
    ppc_cpu_set_msr(0, 0x2000);

    printf("\nStarting execution at PC=0x%08x...\n", TEST_LOAD_ADDR);
    printf("(will trap on illegal instruction after test completes)\n\n");

    // Run the CPU - this will call ppc_start_jitc_asm
    // and enter the JIT loop. It will abort when it hits
    // the illegal instruction (0x00000000) at the end.
    ppc_cpu_run();

    // If we get here, execution completed (stopped by exception)
    printf("\nExecution stopped.\n");
    printf("r3 = %d (expect 10)\n", ppc_cpu_get_gpr(0, 3));
    printf("r4 = %d (expect 10)\n", ppc_cpu_get_gpr(0, 4));
    printf("r5 = 0x%x (expect 0x42)\n", ppc_cpu_get_gpr(0, 5));

    int result = (ppc_cpu_get_gpr(0, 3) == 10 &&
                  ppc_cpu_get_gpr(0, 5) == 0x42) ? 0 : 1;
    printf("\n%s\n", result == 0 ? "PASS" : "FAIL");
    return result;
}
