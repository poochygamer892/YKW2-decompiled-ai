#!/usr/bin/env python3
"""Inspect XQ32 script tables without decompiling or exporting bytecode.

XQ32 stores table offsets in 4-byte units.  Each table may be raw or wrapped
in a Level-5 compression stream.  This utility validates the header, records
compression metadata and checks the decoded table sizes against the two entry
layouts used by Level-5 script readers.  It intentionally emits aggregate
metadata only; script names and instructions are not written to the report.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

from inspect_arc0 import Arc0Error, decompress_level5


class Xq32Error(RuntimeError):
    """Raised when an XQ32 header or table is inconsistent."""


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


TABLE_LAYOUTS = {
    "function": (0x18, 0x20),
    "jump": (0x0C, 0x10),
    "instruction": (0x0C, 0x10),
    "argument": (0x08, 0x10),
}


def table_span(data: bytes, start: int, end: int, label: str) -> tuple[bytes, dict]:
    if start < 0 or end < start or end > len(data):
        raise Xq32Error(f"{label}: invalid span {start:#x}..{end:#x}")
    region = data[start:end]
    if not region:
        return b"", {"method": "empty", "method_id": -1, "compressed_bytes": 0}

    # A raw table has no Level-5 header.  Otherwise the shared decoder gives
    # us both the method and the decoded byte count.
    if len(region) >= 4:
        try:
            decoded, compression = decompress_level5(region, label)
        except (Arc0Error, ValueError, IndexError):
            return region, {
                "method": "raw",
                "method_id": -1,
                "compressed_bytes": len(region),
            }
        return decoded, compression
    return region, {
        "method": "raw",
        "method_id": -1,
        "compressed_bytes": len(region),
    }


def parse_xq32(data: bytes) -> dict:
    if len(data) < 0x18 or data[:4] != b"XQ32":
        raise Xq32Error("missing XQ32 header")

    counts = {
        "function": u16(data, 0x04),
        "jump": u16(data, 0x0A),
        "instruction": u16(data, 0x0E),
        "argument": u16(data, 0x12),
    }
    offsets = {
        "function": u16(data, 0x06) * 4,
        "jump": u16(data, 0x08) * 4,
        "instruction": u16(data, 0x0C) * 4,
        "argument": u16(data, 0x10) * 4,
        "strings": u16(data, 0x16) * 4,
    }
    global_variables = u16(data, 0x14)
    ordered_starts = [offsets[name] for name in ("function", "jump", "instruction", "argument", "strings")]
    if any(start < 0x18 or start > len(data) for start in ordered_starts):
        raise Xq32Error("table offset outside XQ32 file")
    if ordered_starts != sorted(ordered_starts):
        raise Xq32Error("table offsets are not monotonic")

    ends = {
        "function": offsets["jump"],
        "jump": offsets["instruction"],
        "instruction": offsets["argument"],
        "argument": offsets["strings"],
        "strings": len(data),
    }
    tables = {}
    for name in ("function", "jump", "instruction", "argument", "strings"):
        decoded, compression = table_span(data, offsets[name], ends[name], name)
        expected_sizes = []
        pointer_size = None
        if name in TABLE_LAYOUTS and counts[name]:
            expected_sizes = [counts[name] * size for size in TABLE_LAYOUTS[name]]
            if len(decoded) in expected_sizes:
                pointer_size = expected_sizes.index(len(decoded))
                pointer_size = TABLE_LAYOUTS[name][pointer_size]
        tables[name] = {
            "entry_count": counts.get(name, 0),
            "region_bytes": ends[name] - offsets[name],
            "decoded_bytes": len(decoded),
            "compression": compression["method"],
            "compression_id": compression["method_id"],
            "expected_entry_sizes": expected_sizes,
            "detected_entry_size": pointer_size,
            "size_matches_layout": not expected_sizes or len(decoded) in expected_sizes,
        }

    return {
        "function_count": counts["function"],
        "jump_count": counts["jump"],
        "instruction_count": counts["instruction"],
        "argument_count": counts["argument"],
        "global_variable_count": global_variables,
        "offsets": offsets,
        "tables": tables,
    }


def update_table_stats(stats: dict, table: dict) -> None:
    stats["files"] += 1
    stats["compression_methods"][table["compression"]] += 1
    if table["detected_entry_size"] is not None:
        stats["detected_entry_sizes"][str(table["detected_entry_size"])] += 1
    stats["decoded_bytes"] += table["decoded_bytes"]
    stats["region_bytes"] += table["region_bytes"]
    stats["layout_mismatches"] += int(not table["size_matches_layout"])


def normalize_stats(stats: dict) -> dict:
    normalized = {}
    for name, value in stats.items():
        normalized[name] = {
            "files": value["files"],
            "compression_methods": dict(sorted(value["compression_methods"].items())),
            "detected_entry_sizes": dict(sorted(value["detected_entry_sizes"].items())),
            "decoded_bytes": value["decoded_bytes"],
            "region_bytes": value["region_bytes"],
            "layout_mismatches": value["layout_mismatches"],
        }
    return normalized


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, help="decrypted 3DS CCI/NCSD image")
    parser.add_argument("--romfs-path", default="/yw2_a.fa")
    parser.add_argument("--arc0-manifest", required=True, type=Path)
    parser.add_argument("--max-files", type=int, default=0, help="0 means all XQ32 files")
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    try:
        from pyctr.crypto import CryptoEngine
        from pyctr.type.cci import CCIReader, CCISection
        from pyctr.type.ncch import NCCHReader
    except ImportError:
        print("error: --rom requires pyctr; install tools/requirements-3ds.txt", file=sys.stderr)
        return 2

    manifest = json.loads(args.arc0_manifest.read_text(encoding="utf-8"))
    candidates = [item for item in manifest["files"] if item["path"].lower().endswith(".xq")]
    if args.max_files > 0:
        candidates = candidates[: args.max_files]

    table_stats = {
        name: {
            "files": 0,
            "compression_methods": Counter(),
            "detected_entry_sizes": Counter(),
            "decoded_bytes": 0,
            "region_bytes": 0,
            "layout_mismatches": 0,
        }
        for name in ("function", "jump", "instruction", "argument", "strings")
    }
    header_counts = Counter()
    global_variables = Counter()
    errors = Counter()
    processed = 0

    with CCIReader(args.rom, load_contents=False) as cci:
        ncch = NCCHReader(
            cci.open_raw_section(CCISection.Application),
            crypto=CryptoEngine(setup_b9_keys=False),
            assume_decrypted=True,
        )
        try:
            parent = ncch.romfs.open(args.romfs_path)
            try:
                for item in candidates:
                    parent.seek(item["offset"])
                    data = parent.read(item["size"])
                    try:
                        report = parse_xq32(data)
                    except Xq32Error as exc:
                        errors[str(exc)] += 1
                        continue
                    processed += 1
                    header_counts[
                        (
                            report["function_count"],
                            report["jump_count"],
                            report["instruction_count"],
                            report["argument_count"],
                        )
                    ] += 1
                    global_variables[str(report["global_variable_count"])] += 1
                    for name, table in report["tables"].items():
                        update_table_stats(table_stats[name], table)
            finally:
                parent.close()
        finally:
            ncch.close()

    report = {
        "source": "ARC0 XQ32 metadata only",
        "romfs_path": args.romfs_path,
        "candidate_files": len(candidates),
        "xq32_files_processed": processed,
        "errors": dict(errors),
        "header_count_patterns": [
            {"function": key[0], "jump": key[1], "instruction": key[2], "argument": key[3], "files": count}
            for key, count in header_counts.most_common(20)
        ],
        "global_variable_count_distribution": dict(global_variables.most_common(20)),
        "tables": normalize_stats(table_stats),
    }
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
