#!/usr/bin/env python3
"""Inspect nested Level-5 XPCK indexes without extracting their contents."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

from inspect_arc0 import decompress_level5


class XpckError(RuntimeError):
    """Raised when an XPCK header or table is inconsistent."""


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def read_at(stream, base: int, offset: int, size: int) -> bytes:
    stream.seek(base + offset)
    data = stream.read(size)
    if len(data) != size:
        raise XpckError(f"short read at {offset:#x}: expected {size}, got {len(data)}")
    return data


def c_string(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise XpckError(f"name offset outside table: {offset:#x}")
    end = data.find(b"\0", offset)
    if end < 0:
        raise XpckError(f"unterminated name at {offset:#x}")
    return data[offset:end].decode("ascii", errors="replace")


def inspect_xpck(stream, base: int, file_size: int) -> dict:
    header = read_at(stream, base, 0, 0x14)
    if header[:4] != b"XPCK":
        raise XpckError("missing XPCK magic")

    file_count_and_type = u16(header, 4)
    entry_count = file_count_and_type & 0x0FFF
    content_type = file_count_and_type >> 12
    info_offset = u16(header, 6) * 4
    name_offset = u16(header, 8) * 4
    data_offset = u16(header, 10) * 4
    info_size = u16(header, 12) * 4
    name_size = u16(header, 14) * 4
    data_size = u32(header, 16) * 4

    if entry_count * 12 > info_size:
        raise XpckError("entry count exceeds XPCK info table")
    if data_offset + data_size > file_size + 4:
        raise XpckError("XPCK data region exceeds parent file")

    info = read_at(stream, base, info_offset, entry_count * 12)
    compressed_names = read_at(stream, base, name_offset, name_size)
    names, compression = decompress_level5(compressed_names, "XPCK names")
    nested_extensions = Counter()
    names_seen = set()
    for index in range(entry_count):
        entry_offset = index * 12
        name_offset_in_table = u16(info, entry_offset + 4)
        file_offset = ((info[entry_offset + 10] << 16) | u16(info, entry_offset + 6)) * 4
        nested_size = (info[entry_offset + 11] << 16) | u16(info, entry_offset + 8)
        if file_offset + nested_size > data_size + 4:
            raise XpckError(f"nested entry {index} exceeds XPCK data region")
        name = c_string(names, name_offset_in_table)
        names_seen.add(name)
        nested_extensions[Path(name).suffix.lower() or "<none>"] += 1

    return {
        "entry_count": entry_count,
        "content_type": content_type,
        "info_offset": info_offset,
        "name_offset": name_offset,
        "data_offset": data_offset,
        "info_size": info_size,
        "name_size": name_size,
        "data_size": data_size,
        "name_compression": compression["method"],
        "unique_names": len(names_seen),
        "nested_extensions": dict(sorted(nested_extensions.items())),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True)
    parser.add_argument("--romfs-path", default="/yw2_a.fa")
    parser.add_argument("--arc0-manifest", required=True, type=Path)
    parser.add_argument("--max-files", type=int, default=0, help="0 means all XPCK files")
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
    candidates = [item for item in manifest["files"] if item["path"].lower().endswith((".xc", ".xa", ".pck", ".xr", ".xv", ".xf", ".xk", ".efp3ds"))]
    if args.max_files > 0:
        candidates = candidates[: args.max_files]

    content_types = Counter()
    nested_extensions = Counter()
    name_compressions = Counter()
    entry_counts = Counter()
    processed = 0
    skipped = 0
    errors = Counter()

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
                    magic = parent.read(4)
                    if magic != b"XPCK":
                        skipped += 1
                        continue
                    try:
                        info = inspect_xpck(parent, item["offset"], item["size"])
                    except XpckError as exc:
                        errors[str(exc)] += 1
                        continue
                    processed += 1
                    content_types[str(info["content_type"])] += 1
                    name_compressions[info["name_compression"]] += 1
                    entry_counts[str(info["entry_count"])] += 1
                    nested_extensions.update(info["nested_extensions"])
            finally:
                parent.close()
        finally:
            ncch.close()

    report = {
        "source": "ARC0 XPCK metadata only",
        "romfs_path": args.romfs_path,
        "candidate_files": len(candidates),
        "xpck_files_processed": processed,
        "non_xpck_candidates": skipped,
        "errors": dict(errors),
        "total_nested_entries": sum(int(count) * int(entry_count) for entry_count, count in entry_counts.items()),
        "content_types": dict(sorted(content_types.items())),
        "name_compressions": dict(sorted(name_compressions.items())),
        "entry_count_distribution": dict(entry_counts.most_common(20)),
        "nested_extensions": dict(sorted(nested_extensions.items())),
    }
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
