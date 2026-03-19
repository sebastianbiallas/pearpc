---
name: check-parity
description: Check that aarch64 JIT interpreter functions match generic CPU behavior
allowed-tools: Bash, Read, Grep
argument-hint: [opcode-name]
---

# Check Interpreter Parity

Run the interpreter parity check and analyze results. If an opcode name is given, show a side-by-side diff for that specific opcode.

## Steps

0. **Always run the stub audit first** — this catches no-op stubs (the #1 bug source):
   ```
   python3 scripts/debug/stub_audit.py $ARGUMENTS
   ```
   If an opcode name was given, this shows a side-by-side generic vs JIT diff.
   If no arguments, it scans all functions and reports no-op stubs.

1. Run `./scripts/debug/check_interpreter_parity.sh`
2. For any issues found:
   - **Commented-out ppc_exception()**: These are bugs. The exception is silently dropped. Enable the call.
   - **GEN_INTERPRET with ppc_exception()**: The exception will be silently dropped because GEN_INTERPRET doesn't check exception_pending. Need native gen_ function or change to GEN_INTERPRET_BRANCH/ENDBLOCK (but ENDBLOCK also doesn't work for synchronous exceptions — see doc/AGENT_DEBUGGING.md).
   - **Missing MSR_POW handling**: Must strip POW bit in ppc_set_msr().

3. For each GEN_INTERPRET warning, check the x86 JIT (`src/cpu/cpu_jitc_x86/ppc_opc.cc`) to see how it handles the same opcode. The x86 JIT is the reference for how gen_ functions should work.

## Critical rule

GEN_INTERPRET CANNOT dispatch synchronous exceptions. The only correct approaches are:
- Native gen_ that jumps directly to ppc_program_exception_asm (like x86 JIT does)
- GEN_INTERPRET_BRANCH which dispatches to npc (works but forces a dispatch on every call)
