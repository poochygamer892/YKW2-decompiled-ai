#!/usr/bin/env python3
"""Profile structural XQ32 invariants without exporting script contents.

The report contains counts and token frequencies only. It does not write paths,
symbols, arguments, instructions or generated code, so it is suitable for
keeping the ABI-recovery method reproducible without putting game data in Git.
"""

from __future__ import annotations

import argparse
import collections
import json
import struct
import sys
import zlib
from pathlib import Path

from inspect_arc0 import Arc0Error, decompress_level5


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def decoded_table(data: bytes, start: int, end: int, name: str) -> bytes:
    region = data[start:end]
    if not region:
        return b""
    try:
        decoded, _ = decompress_level5(region, name)
        return decoded
    except (Arc0Error, IndexError, ValueError):
        return region


def profile_program(data: bytes, stats: dict) -> None:
    offsets = [u16(data, offset) * 4 for offset in (6, 8, 12, 16, 22)]
    counts = {
        "function": u16(data, 4),
        "jump": u16(data, 10),
        "instruction": u16(data, 14),
        "argument": u16(data, 18),
    }
    spans = offsets + [len(data)]
    tables = [
        decoded_table(data, spans[index], spans[index + 1], name)
        for index, name in enumerate(("function", "jump", "instruction", "argument", "strings"))
    ]
    functions, _, instructions, arguments, strings = tables
    instruction_rows = [
        struct.unpack_from("<III", instructions, offset)
        for offset in range(0, len(instructions) - 11, 12)
    ]
    argument_rows = [
        struct.unpack_from("<II", arguments, offset)
        for offset in range(0, len(arguments) - 7, 8)
    ]
    function_rows = [
        struct.unpack_from("<IIIIII", functions, offset)
        for offset in range(0, len(functions) - 23, 24)
    ]
    stats["files"] += 1
    stats["header_counts"][str(tuple(counts[name] for name in ("function", "jump", "instruction", "argument")))] += 1
    stats["instruction_rows"] += len(instruction_rows)
    stats["argument_rows"] += len(argument_rows)
    stats["function_rows"] += len(function_rows)
    for first, operation, _ in instruction_rows:
        argument_index = first & 0xFFFF
        argument_count = first >> 16
        if argument_index + argument_count <= len(argument_rows):
            stats["argument_spans_valid"] += 1
        else:
            stats["argument_spans_invalid"] += 1
        stats["operation_tokens"][str(operation)] += 1
    for _, _, flags in instruction_rows:
        stats["instruction_flags"][str(flags)] += 1
    symbols = {item for item in strings.split(b"\0") if item}
    symbol_hashes = {zlib.crc32(item) & 0xFFFFFFFF for item in symbols}
    for instruction_index, name_hash, *_ in function_rows:
        if instruction_index < len(instruction_rows):
            stats["function_starts_valid"] += 1
        else:
            stats["function_starts_invalid"] += 1
        stats["function_hash_matches"] += int(name_hash in symbol_hashes)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, help="decrypted CCI/3DS image")
    parser.add_argument("--arc0-manifest", required=True, type=Path)
    parser.add_argument("--romfs-path", default="/yw2_a.fa")
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()
    try:
        from pyctr.crypto import CryptoEngine
        from pyctr.type.cci import CCIReader, CCISection
        from pyctr.type.ncch import NCCHReader
    except ImportError:
        print("error: install tools/requirements-3ds.txt", file=sys.stderr)
        return 2

    manifest = json.loads(args.arc0_manifest.read_text(encoding="utf-8"))
    stats = {
        "files": 0,
        "header_counts": collections.Counter(),
        "instruction_rows": 0,
        "argument_rows": 0,
        "function_rows": 0,
        "argument_spans_valid": 0,
        "argument_spans_invalid": 0,
        "function_starts_valid": 0,
        "function_starts_invalid": 0,
        "function_hash_matches": 0,
        "instruction_flags": collections.Counter(),
        "operation_tokens": collections.Counter(),
    }
    candidates = [item for item in manifest["files"] if item["path"].lower().endswith(".xq")]
    with CCIReader(args.rom, load_contents=False) as cci:
        ncch = NCCHReader(
            cci.open_raw_section(CCISection.Application),
            crypto=CryptoEngine(setup_b9_keys=False),
            assume_decrypted=True,
        )
        with ncch.romfs.open(args.romfs_path) as parent:
            for item in candidates:
                parent.seek(item["offset"])
                profile_program(parent.read(item["size"]), stats)

    report = {
        "source": "authorized XQ32 structural profile",
        "files": stats["files"],
        "rows": {key: stats[key] for key in ("function_rows", "instruction_rows", "argument_rows")},
        "argument_spans": {key: stats[key] for key in ("argument_spans_valid", "argument_spans_invalid")},
        "function_starts": {key: stats[key] for key in ("function_starts_valid", "function_starts_invalid")},
        "function_hash_matches": stats["function_hash_matches"],
        "instruction_flags": dict(stats["instruction_flags"]),
        "header_count_patterns": dict(stats["header_counts"]),
        "top_operation_tokens": stats["operation_tokens"].most_common(32),
    }
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.json_out:
        args.json_out.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
