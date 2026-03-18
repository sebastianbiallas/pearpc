#!/usr/bin/env python3
"""Basic PPC disassembler for memory dump regions.

Decodes common PPC instructions from a memory dump at a given physical address.

Usage: python3 test/disasm_ppc.py memdump_jit.bin 0x00007b78 32
"""

import sys
import struct
import argparse


# SPR number encoding: SPR field in mfspr/mtspr is split as (spr[5:9], spr[0:4])
SPR_NAMES = {
    1: 'XER',
    8: 'LR',
    9: 'CTR',
    18: 'DSISR',
    19: 'DAR',
    22: 'DEC',
    25: 'SDR1',
    26: 'SRR0',
    27: 'SRR1',
    272: 'SPRG0',
    273: 'SPRG1',
    274: 'SPRG2',
    275: 'SPRG3',
    282: 'EAR',
    287: 'PVR',
    528: 'IBAT0U',
    529: 'IBAT0L',
    530: 'IBAT1U',
    531: 'IBAT1L',
    532: 'IBAT2U',
    533: 'IBAT2L',
    534: 'IBAT3U',
    535: 'IBAT3L',
    536: 'DBAT0U',
    537: 'DBAT0L',
    538: 'DBAT1U',
    539: 'DBAT1L',
    540: 'DBAT2U',
    541: 'DBAT2L',
    542: 'DBAT3U',
    543: 'DBAT3L',
    1008: 'HID0',
    1009: 'HID1',
    1010: 'IABR',
    1013: 'DABR',
    1023: 'PIR',
}

# Condition register bit names for bc instructions
CR_BIT_NAMES = {0: 'lt', 1: 'gt', 2: 'eq', 3: 'so'}


def sext(value, bits):
    """Sign-extend a value from 'bits' width to Python int."""
    if value & (1 << (bits - 1)):
        return value - (1 << bits)
    return value


def decode_spr(spr_field):
    """Decode SPR field from mfspr/mtspr instruction."""
    # SPR encoding swaps the two 5-bit halves
    return ((spr_field & 0x1f) << 5) | ((spr_field >> 5) & 0x1f)


def spr_name(num):
    """Get SPR name or numeric string."""
    return SPR_NAMES.get(num, f'spr{num}')


def decode_insn(insn, addr):
    """Decode a single PPC instruction. Returns mnemonic string."""
    opcode = (insn >> 26) & 0x3f
    rd = (insn >> 21) & 0x1f
    ra = (insn >> 16) & 0x1f
    rb = (insn >> 11) & 0x1f
    rc = insn & 1  # Rc bit
    simm = sext(insn & 0xffff, 16)
    uimm = insn & 0xffff
    xo_10 = (insn >> 1) & 0x3ff  # XO for opcode 31 (10-bit)
    xo_9 = (insn >> 1) & 0x1ff   # XO for opcode 31 (9-bit)

    # nop: ori r0, r0, 0
    if insn == 0x60000000:
        return 'nop'

    # Branch (opcode 18)
    if opcode == 18:
        li = insn & 0x03fffffc
        li = sext(li, 26)
        aa = (insn >> 1) & 1
        lk = insn & 1
        if aa:
            target = li & 0xffffffff
        else:
            target = (addr + li) & 0xffffffff
        mnem = 'b'
        if lk:
            mnem += 'l'
        if aa:
            mnem += 'a'
        return f'{mnem} 0x{target:08x}'

    # Branch conditional (opcode 16)
    if opcode == 16:
        bo = rd
        bi = ra
        bd = sext(insn & 0xfffc, 16)
        aa = (insn >> 1) & 1
        lk = insn & 1
        if aa:
            target = bd & 0xffffffff
        else:
            target = (addr + bd) & 0xffffffff
        # Simplified mnemonics
        cr_field = bi >> 2
        cr_bit = bi & 3
        bit_name = CR_BIT_NAMES.get(cr_bit, f'bit{cr_bit}')
        suffix = 'l' if lk else ''
        suffix += 'a' if aa else ''
        # bdnz (bo=16, bi=0)
        if bo == 16 and bi == 0:
            return f'bdnz{suffix} 0x{target:08x}'
        # bdz (bo=18, bi=0)
        if bo == 18 and bi == 0:
            return f'bdz{suffix} 0x{target:08x}'
        # bt (bo=12)
        if bo == 12:
            if cr_field == 0:
                names = {0: 'blt', 1: 'bgt', 2: 'beq', 3: 'bso'}
                mnem = names.get(cr_bit, f'bt {bi},')
            else:
                mnem = f'b{bit_name} cr{cr_field},'
            return f'{mnem}{suffix} 0x{target:08x}'
        # bf (bo=4)
        if bo == 4:
            if cr_field == 0:
                names = {0: 'bge', 1: 'ble', 2: 'bne', 3: 'bns'}
                mnem = names.get(cr_bit, f'bf {bi},')
            else:
                neg_names = {0: 'bge', 1: 'ble', 2: 'bne', 3: 'bns'}
                mnem = f'{neg_names.get(cr_bit, "bf")} cr{cr_field},'
            return f'{mnem}{suffix} 0x{target:08x}'
        return f'bc{suffix} {bo},{bi},0x{target:08x}'

    # CR ops / bclr / bcctr (opcode 19)
    if opcode == 19:
        xo = (insn >> 1) & 0x3ff
        lk = insn & 1
        if xo == 16:  # bclr
            bo = rd
            suffix = 'l' if lk else ''
            if bo == 20 and ra == 0:
                return f'blr{suffix}'
            return f'bclr{suffix} {bo},{ra}'
        if xo == 528:  # bcctr
            bo = rd
            suffix = 'l' if lk else ''
            if bo == 20 and ra == 0:
                return f'bctr{suffix}' if not lk else f'bctrl'
            return f'bcctr{suffix} {bo},{ra}'
        if xo == 150:
            return 'isync'
        if xo == 0:
            return f'mcrf cr{rd >> 2},cr{ra >> 2}'
        if xo == 33:
            return f'crnor {rd},{ra},{rb}'
        if xo == 129:
            return f'crandc {rd},{ra},{rb}'
        if xo == 193:
            return f'crxor {rd},{ra},{rb}'
        if xo == 225:
            return f'crnand {rd},{ra},{rb}'
        if xo == 257:
            return f'crand {rd},{ra},{rb}'
        if xo == 289:
            return f'creqv {rd},{ra},{rb}'
        if xo == 417:
            return f'crorc {rd},{ra},{rb}'
        if xo == 449:
            return f'cror {rd},{ra},{rb}'
        return f'.long 0x{insn:08x}  # cr op xo={xo}'

    # addi / li (opcode 14)
    if opcode == 14:
        if ra == 0:
            return f'li r{rd},0x{simm & 0xffff:x}' if simm >= 0 else f'li r{rd},{simm}'
        return f'addi r{rd},r{ra},{simm}'

    # addis / lis (opcode 15)
    if opcode == 15:
        if ra == 0:
            return f'lis r{rd},0x{uimm:x}'
        return f'addis r{rd},r{ra},0x{uimm:x}'

    # ori (opcode 24)
    if opcode == 24:
        if rd == ra == 0 and uimm == 0:
            return 'nop'
        return f'ori r{ra},r{rd},0x{uimm:x}'

    # oris (opcode 25)
    if opcode == 25:
        return f'oris r{ra},r{rd},0x{uimm:x}'

    # xori (opcode 26)
    if opcode == 26:
        return f'xori r{ra},r{rd},0x{uimm:x}'

    # xoris (opcode 27)
    if opcode == 27:
        return f'xoris r{ra},r{rd},0x{uimm:x}'

    # andi. (opcode 28)
    if opcode == 28:
        return f'andi. r{ra},r{rd},0x{uimm:x}'

    # andis. (opcode 29)
    if opcode == 29:
        return f'andis. r{ra},r{rd},0x{uimm:x}'

    # cmpwi (opcode 11)
    if opcode == 11:
        cr_field = rd >> 2
        if cr_field == 0:
            return f'cmpwi r{ra},{simm}'
        return f'cmpwi cr{cr_field},r{ra},{simm}'

    # cmplwi (opcode 10)
    if opcode == 10:
        cr_field = rd >> 2
        if cr_field == 0:
            return f'cmplwi r{ra},0x{uimm:x}'
        return f'cmplwi cr{cr_field},r{ra},0x{uimm:x}'

    # addic (opcode 12)
    if opcode == 12:
        return f'addic r{rd},r{ra},{simm}'

    # addic. (opcode 13)
    if opcode == 13:
        return f'addic. r{rd},r{ra},{simm}'

    # subfic (opcode 8)
    if opcode == 8:
        return f'subfic r{rd},r{ra},{simm}'

    # mulli (opcode 7)
    if opcode == 7:
        return f'mulli r{rd},r{ra},{simm}'

    # twi (opcode 3)
    if opcode == 3:
        return f'twi {rd},r{ra},{simm}'

    # rlwinm (opcode 21)
    if opcode == 21:
        sh = rb
        mb = (insn >> 6) & 0x1f
        me = (insn >> 1) & 0x1f
        dot = '.' if rc else ''
        # Simplified forms
        if mb == 0 and me == 31 - sh:
            return f'slwi{dot} r{ra},r{rd},{sh}'
        if sh == 32 - mb and me == 31:
            return f'srwi{dot} r{ra},r{rd},{mb}'
        if sh == 0 and mb == 0:
            return f'clrrwi{dot} r{ra},r{rd},{31 - me}'
        if sh == 0:
            return f'clrlwi{dot} r{ra},r{rd},{mb}'
        return f'rlwinm{dot} r{ra},r{rd},{sh},{mb},{me}'

    # rlwimi (opcode 20)
    if opcode == 20:
        sh = rb
        mb = (insn >> 6) & 0x1f
        me = (insn >> 1) & 0x1f
        dot = '.' if rc else ''
        return f'rlwimi{dot} r{ra},r{rd},{sh},{mb},{me}'

    # rlwnm (opcode 23)
    if opcode == 23:
        mb = (insn >> 6) & 0x1f
        me = (insn >> 1) & 0x1f
        dot = '.' if rc else ''
        return f'rlwnm{dot} r{ra},r{rd},r{rb},{mb},{me}'

    # Load/store integer (opcodes 32-47)
    ls_mnemonics = {
        32: 'lwz', 33: 'lwzu', 34: 'lbz', 35: 'lbzu',
        36: 'stw', 37: 'stwu', 38: 'stb', 39: 'stbu',
        40: 'lhz', 41: 'lhzu', 42: 'lha', 43: 'lhau',
        44: 'sth', 45: 'sthu', 46: 'lmw', 47: 'stmw',
    }
    if opcode in ls_mnemonics:
        mnem = ls_mnemonics[opcode]
        disp = simm
        if ra == 0:
            return f'{mnem} r{rd},0x{disp & 0xffff:x}(0)'
        return f'{mnem} r{rd},0x{disp & 0xffff:x}(r{ra})'

    # Load/store FP (opcodes 48-55)
    fp_ls = {
        48: 'lfs', 49: 'lfsu', 50: 'lfd', 51: 'lfdu',
        52: 'stfs', 53: 'stfsu', 54: 'stfd', 55: 'stfdu',
    }
    if opcode in fp_ls:
        mnem = fp_ls[opcode]
        disp = simm
        if ra == 0:
            return f'{mnem} f{rd},0x{disp & 0xffff:x}(0)'
        return f'{mnem} f{rd},0x{disp & 0xffff:x}(r{ra})'

    # sc (opcode 17)
    if opcode == 17:
        return 'sc'

    # Extended opcodes (opcode 31)
    if opcode == 31:
        xo_full = (insn >> 1) & 0x3ff
        xo_short = (insn >> 1) & 0x1ff
        dot = '.' if rc else ''
        oe = (insn >> 10) & 1

        # Comparison
        if xo_full == 0:
            cr_field = rd >> 2
            if cr_field == 0:
                return f'cmp r{ra},r{rb}'
            return f'cmp cr{cr_field},r{ra},r{rb}'
        if xo_full == 32:
            cr_field = rd >> 2
            if cr_field == 0:
                return f'cmpl r{ra},r{rb}'
            return f'cmpl cr{cr_field},r{ra},r{rb}'

        # ALU with OE
        alu_oe = {
            266: 'add', 10: 'addc', 138: 'adde',
            40: 'subf', 8: 'subfc', 136: 'subfe',
            234: 'addme', 202: 'addze',
            232: 'subfme', 200: 'subfze',
            235: 'mullw', 491: 'divw', 459: 'divwu',
            104: 'neg',
        }
        if xo_short in alu_oe:
            mnem = alu_oe[xo_short]
            o = 'o' if oe else ''
            if xo_short == 104:  # neg
                return f'{mnem}{o}{dot} r{rd},r{ra}'
            return f'{mnem}{o}{dot} r{rd},r{ra},r{rb}'

        # Logic
        if xo_full == 28:
            if rd == rb:
                return f'mr{dot} r{ra},r{rd}'
            return f'and{dot} r{ra},r{rd},r{rb}'
        if xo_full == 444:
            if rd == rb:
                return f'mr{dot} r{ra},r{rd}'
            return f'or{dot} r{ra},r{rd},r{rb}'
        if xo_full == 316:
            return f'xor{dot} r{ra},r{rd},r{rb}'
        if xo_full == 476:
            return f'nand{dot} r{ra},r{rd},r{rb}'
        if xo_full == 124:
            return f'nor{dot} r{ra},r{rd},r{rb}'
        if xo_full == 284:
            return f'eqv{dot} r{ra},r{rd},r{rb}'
        if xo_full == 60:
            return f'andc{dot} r{ra},r{rd},r{rb}'
        if xo_full == 412:
            return f'orc{dot} r{ra},r{rd},r{rb}'

        # Shifts
        if xo_full == 24:
            return f'slw{dot} r{ra},r{rd},r{rb}'
        if xo_full == 536:
            return f'srw{dot} r{ra},r{rd},r{rb}'
        if xo_full == 792:
            return f'sraw{dot} r{ra},r{rd},r{rb}'
        if xo_full == 824:
            sh = rb
            return f'srawi{dot} r{ra},r{rd},{sh}'

        # Extend
        if xo_full == 26:
            return f'cntlzw{dot} r{ra},r{rd}'
        if xo_full == 954:
            return f'extsb{dot} r{ra},r{rd}'
        if xo_full == 922:
            return f'extsh{dot} r{ra},r{rd}'

        # Load/store indexed
        ls_x = {
            23: 'lwzx', 55: 'lwzux',
            87: 'lbzx', 119: 'lbzux',
            151: 'stwx', 183: 'stwux',
            215: 'stbx', 247: 'stbux',
            279: 'lhzx', 311: 'lhzux',
            343: 'lhax', 375: 'lhaux',
            407: 'sthx', 439: 'sthux',
            20: 'lwarx', 150: 'stwcx.',
            534: 'lwbrx', 662: 'stwbrx',
            790: 'lhbrx', 918: 'sthbrx',
        }
        if xo_full in ls_x:
            mnem = ls_x[xo_full]
            return f'{mnem} r{rd},r{ra},r{rb}'

        # FP load/store indexed
        fp_x = {
            535: 'lfsx', 567: 'lfsux',
            599: 'lfdx', 631: 'lfdux',
            663: 'stfsx', 695: 'stfsux',
            727: 'stfdx', 759: 'stfdux',
        }
        if xo_full in fp_x:
            mnem = fp_x[xo_full]
            return f'{mnem} f{rd},r{ra},r{rb}'

        # SPR moves
        if xo_full == 339:  # mfspr
            spr = decode_spr((insn >> 11) & 0x3ff)
            return f'mfspr r{rd},{spr_name(spr)}'
        if xo_full == 467:  # mtspr
            spr = decode_spr((insn >> 11) & 0x3ff)
            return f'mtspr {spr_name(spr)},r{rd}'

        # CR
        if xo_full == 19:  # mfcr
            return f'mfcr r{rd}'
        if xo_full == 144:  # mtcrf
            crm = (insn >> 12) & 0xff
            return f'mtcrf 0x{crm:02x},r{rd}'

        # TLB / cache
        if xo_full == 86:
            return f'dcbf r{ra},r{rb}'
        if xo_full == 470:
            return f'dcbi r{ra},r{rb}'
        if xo_full == 54:
            return f'dcbst r{ra},r{rb}'
        if xo_full == 278:
            return f'dcbt r{ra},r{rb}'
        if xo_full == 246:
            return f'dcbtst r{ra},r{rb}'
        if xo_full == 1014:
            return f'dcbz r{ra},r{rb}'
        if xo_full == 982:
            return f'icbi r{ra},r{rb}'
        if xo_full == 566:
            return f'tlbsync'
        if xo_full == 370:
            return f'tlbia'
        if xo_full == 306:
            return f'tlbie r{rb}'
        if xo_full == 498:
            return f'tlbld r{rb}'
        if xo_full == 530:
            return f'tlbli r{rb}'

        # Sync / eieio
        if xo_full == 598:
            return 'sync'
        if xo_full == 854:
            return 'eieio'

        # Segment registers
        if xo_full == 210:
            return f'mtsr {ra},r{rd}'
        if xo_full == 242:
            return f'mtsrin r{rd},r{rb}'
        if xo_full == 595:
            return f'mfsr r{rd},{ra}'
        if xo_full == 659:
            return f'mfsrin r{rd},r{rb}'

        # rfi is actually opcode 19, but handle mfmsr/mtmsr here
        if xo_full == 83:
            return f'mfmsr r{rd}'
        if xo_full == 146:
            return f'mtmsr r{rd}'

        # dssall (AltiVec)
        if xo_full == 822:
            return 'dssall'

        return f'.long 0x{insn:08x}  # op31 xo={xo_full}'

    # rfi (opcode 19, but already handled above... it's actually opcode 19 xo=50)
    # Actually rfi is opcode 19 xo=50, handled in CR ops section
    # Let's catch it for safety
    if opcode == 19 and ((insn >> 1) & 0x3ff) == 50:
        return 'rfi'

    # FP ops (opcode 59 - single, opcode 63 - double)
    if opcode == 59:
        xo5 = (insn >> 1) & 0x1f
        dot = '.' if rc else ''
        fp_59 = {18: 'fdivs', 20: 'fsubs', 21: 'fadds', 25: 'fmuls',
                 28: 'fmsubs', 29: 'fmadds', 30: 'fnmsubs', 31: 'fnmadds',
                 24: 'fres'}
        if xo5 in fp_59:
            return f'{fp_59[xo5]}{dot} f{rd},f{ra},f{rb}'
        return f'.long 0x{insn:08x}  # fp59 xo={xo5}'

    if opcode == 63:
        xo_full = (insn >> 1) & 0x3ff
        xo5 = (insn >> 1) & 0x1f
        dot = '.' if rc else ''
        # 10-bit XO
        fp_63_10 = {
            0: 'fcmpu', 12: 'frsp', 14: 'fctiw', 15: 'fctiwz',
            32: 'fcmpo', 38: 'mtfsb1', 40: 'fneg', 64: 'mcrfs',
            70: 'mtfsb0', 72: 'fmr', 134: 'mtfsfi', 264: 'fabs',
            583: 'mffs', 711: 'mtfsf', 136: 'fnabs',
        }
        if xo_full in fp_63_10:
            return f'{fp_63_10[xo_full]}{dot} f{rd},f{rb}'
        # 5-bit XO
        fp_63_5 = {18: 'fdiv', 20: 'fsub', 21: 'fadd', 23: 'fsel',
                   25: 'fmul', 26: 'frsqrte',
                   28: 'fmsub', 29: 'fmadd', 30: 'fnmsub', 31: 'fnmadd'}
        if xo5 in fp_63_5:
            return f'{fp_63_5[xo5]}{dot} f{rd},f{ra},f{rb}'
        return f'.long 0x{insn:08x}  # fp63 xo10={xo_full} xo5={xo5}'

    return f'.long 0x{insn:08x}'


def main():
    parser = argparse.ArgumentParser(
        description='Disassemble PPC instructions from a memory dump')
    parser.add_argument('dumpfile', help='Memory dump file')
    parser.add_argument('address', help='Physical address to start disassembly (hex)')
    parser.add_argument('count', nargs='?', type=int, default=16,
                        help='Number of instructions to disassemble (default: 16)')
    parser.add_argument('--pa', action='store_true',
                        help='Show raw PA instead of VA (PA + 0xc0000000)')
    args = parser.parse_args()

    pa = int(args.address, 0)

    try:
        with open(args.dumpfile, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.dumpfile}", file=sys.stderr)
        sys.exit(1)

    if pa >= len(data):
        print(f"Error: address 0x{pa:08x} is beyond dump size (0x{len(data):08x})",
              file=sys.stderr)
        sys.exit(1)

    print(f"Disassembly at PA=0x{pa:08x}, {args.count} instructions:")
    print()

    for i in range(args.count):
        offset = pa + i * 4
        if offset + 4 > len(data):
            print(f"  (end of dump)")
            break
        insn = struct.unpack_from('>I', data, offset)[0]
        if args.pa:
            addr_str = f'{offset:08x}'
        else:
            va = offset + 0xc0000000
            addr_str = f'{va:08x}'
        decoded = decode_insn(insn, offset if args.pa else offset + 0xc0000000)
        print(f'  {addr_str}:  {insn:08x}    {decoded}')


if __name__ == '__main__':
    main()
