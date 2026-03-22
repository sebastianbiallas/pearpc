#!/usr/bin/env python3
"""Extract the Linux kernel printk log buffer from a PearPC memory dump.

Searches for the kernel log buffer by looking for printk-style messages
(e.g., '<4>Linux version' or '<6>Linux version') and prints all messages
found. Messages are NUL-terminated strings with <N> priority prefixes.

Usage: python3 test/dump_kernel_log.py memdump_jit.bin
"""

import sys
import struct
import argparse
import re


def find_log_buffer(data):
    """Search for the kernel log buffer start by looking for known patterns."""
    patterns = [
        b'<4>Linux version ',
        b'<6>Linux version ',
        b'<5>Linux version ',
    ]
    for pat in patterns:
        idx = data.find(pat)
        if idx != -1:
            return idx
    return -1


def find_log_buffer_boundaries(data, start):
    """Walk backwards from the found pattern to find the true buffer start,
    and forward to find the end."""
    # Walk backwards: look for the start of printk messages before our hit.
    # The log buffer is a contiguous region of NUL-terminated strings.
    # Walk back to find where printk messages begin.
    buf_start = start
    while buf_start > 0:
        # Look for a NUL byte that precedes another printk <N> tag
        prev_nul = data.rfind(b'\x00', 0, buf_start)
        if prev_nul == -1:
            break
        # Check if the byte after the NUL looks like a printk prefix
        candidate = prev_nul + 1
        if candidate < len(data) - 2:
            if data[candidate:candidate + 1] == b'<' and \
               data[candidate + 1:candidate + 2] in b'0123456789' and \
               data[candidate + 2:candidate + 3] == b'>':
                buf_start = candidate
                continue
        # No more printk messages found before this point
        break

    # Walk forward to find the end of the log buffer.
    # The buffer ends when we hit a region of NUL bytes (no more messages).
    buf_end = start
    while buf_end < len(data):
        # Find next NUL
        next_nul = data.find(b'\x00', buf_end)
        if next_nul == -1:
            buf_end = len(data)
            break
        # Check if there's another message after this NUL
        candidate = next_nul + 1
        if candidate >= len(data) - 2:
            buf_end = next_nul + 1
            break
        # If next bytes are NUL or not a printk prefix, we might be at the end
        if data[candidate:candidate + 1] == b'<' and \
           data[candidate + 1:candidate + 2] in b'0123456789' and \
           data[candidate + 2:candidate + 3] == b'>':
            buf_end = candidate
            continue
        # Could also be a continuation without a priority prefix (rare)
        # Check if there's printable ASCII
        if data[candidate:candidate + 1] == b'\x00':
            buf_end = next_nul + 1
            break
        # Non-printk text after NUL - might be padding or end of buffer
        buf_end = next_nul + 1
        break

    return buf_start, buf_end


def extract_messages(data, start, end):
    """Extract individual printk messages from the buffer region."""
    messages = []
    region = data[start:end]
    # Split on NUL bytes
    parts = region.split(b'\x00')
    for part in parts:
        if not part:
            continue
        try:
            msg = part.decode('ascii', errors='replace')
            messages.append(msg)
        except Exception:
            continue
    return messages


def scan_all_printk(data):
    """Scan the entire dump for printk-style messages as a fallback."""
    messages = []
    pattern = re.compile(rb'<[0-9]>[^\x00]{4,}')
    for m in pattern.finditer(data):
        start = m.start()
        # Read until NUL or non-printable
        end = start
        while end < len(data) and data[end] != 0:
            end += 1
        try:
            msg = data[start:end].decode('ascii', errors='replace')
            messages.append((start, msg))
        except Exception:
            continue
    return messages


def main():
    parser = argparse.ArgumentParser(
        description='Extract Linux kernel printk log from PearPC memory dump')
    parser.add_argument('dumpfile', help='Memory dump file (e.g., memdump_jit.bin)')
    parser.add_argument('--scan', action='store_true',
                        help='Scan entire dump for all printk messages')
    parser.add_argument('--raw', action='store_true',
                        help='Show raw addresses alongside messages')
    args = parser.parse_args()

    try:
        with open(args.dumpfile, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.dumpfile}", file=sys.stderr)
        sys.exit(1)

    print(f"Dump file: {args.dumpfile} ({len(data)} bytes)")

    if args.scan:
        print("\nScanning entire dump for printk messages...\n")
        messages = scan_all_printk(data)
        if not messages:
            print("No printk messages found.")
            sys.exit(1)
        for pa, msg in messages:
            va = pa + 0xc0000000 if pa >= 0x01400000 else pa
            if args.raw:
                print(f"  [PA={pa:08x} VA={va:08x}] {msg}")
            else:
                print(f"  {msg}")
        print(f"\nTotal: {len(messages)} messages found")
        return

    # Find the log buffer
    log_start = find_log_buffer(data)
    if log_start == -1:
        print("Could not find kernel log buffer (no 'Linux version' printk found).")
        print("Try --scan to search for all printk-style messages.")
        sys.exit(1)

    print(f"Found log buffer marker at PA=0x{log_start:08x}")

    buf_start, buf_end = find_log_buffer_boundaries(data, log_start)
    print(f"Log buffer region: PA=0x{buf_start:08x} - 0x{buf_end:08x} "
          f"({buf_end - buf_start} bytes)")

    messages = extract_messages(data, buf_start, buf_end)
    if not messages:
        print("No messages extracted. Try --scan mode.")
        sys.exit(1)

    print(f"\n--- Kernel log ({len(messages)} messages) ---\n")
    for msg in messages:
        print(msg)


if __name__ == '__main__':
    main()
