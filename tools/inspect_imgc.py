#!/usr/bin/env python3
"""Fingerprint IMGC image headers without decoding or exporting textures.

The exact meaning of every IMGC field is not assumed here.  The report keeps
field offsets explicit, which lets a later renderer implementation promote a
field to a named concept only after it has been cross-validated.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path


class ImgcError(RuntimeError):
    """Raised when an IMGC header is too short or has an unexpected magic."""


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_imgc(data: bytes) -> dict:
    if len(data) < 0x40 or data[:4] != b"IMGC":
        raise ImgcError("missing IMGC header")
    return {
        "signature8": data[:8].hex(),
        "field_0x08": u32(data, 0x08),
        "field_0x0c": u32(data, 0x0C),
        "field_0x0e_u16": u16(data, 0x0E),
        "field_0x10_u16": u16(data, 0x10),
        "field_0x12_u16": u16(data, 0x12),
        "field_0x10_0x12_u16_pair": f"{u16(data, 0x10)}x{u16(data, 0x12)}",
        "field_0x14": u32(data, 0x14),
        "field_0x18_u16": u16(data, 0x18),
        "field_0x1a_u16": u16(data, 0x1A),
        "field_0x1c": u32(data, 0x1C),
        "field_0x20": u32(data, 0x20),
        "field_0x34": u32(data, 0x34),
        "field_0x38": u32(data, 0x38),
        "field_0x3c": u32(data, 0x3C),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, help="decrypted 3DS CCI/NCSD image")
    parser.add_argument("--romfs-path", default="/yw2_a.fa")
    parser.add_argument("--arc0-manifest", required=True, type=Path)
    parser.add_argument("--max-files", type=int, default=0, help="0 means all IMGC files")
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
    candidates = [item for item in manifest["files"] if item["path"].lower().endswith(".xi")]
    if args.max_files > 0:
        candidates = candidates[: args.max_files]

    field_values = {name: Counter() for name in (
        "signature8", "field_0x08", "field_0x0c", "field_0x0e_u16",
        "field_0x10_u16", "field_0x12_u16", "field_0x10_0x12_u16_pair", "field_0x14", "field_0x18_u16",
        "field_0x1a_u16", "field_0x1c", "field_0x20", "field_0x34", "field_0x38", "field_0x3c",
    )}
    signature_lengths = Counter()
    errors = Counter()
    processed = 0
    total_bytes = 0

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
                    data = parent.read(min(item["size"], 0x40))
                    try:
                        parsed = parse_imgc(data)
                    except ImgcError as exc:
                        errors[str(exc)] += 1
                        continue
                    processed += 1
                    total_bytes += item["size"]
                    signature_lengths[str(item["size"])] += 1
                    for name, value in parsed.items():
                        field_values[name][str(value)] += 1
            finally:
                parent.close()
        finally:
            ncch.close()

    report = {
        "source": "ARC0 IMGC header metadata only",
        "romfs_path": args.romfs_path,
        "candidate_files": len(candidates),
        "imgc_files_processed": processed,
        "total_payload_bytes": total_bytes,
        "errors": dict(errors),
        "field_invariants": {
            name: {"distinct_values": len(values), "top_values": dict(values.most_common(12))}
            for name, values in field_values.items()
        },
        "file_size_distribution": dict(signature_lengths.most_common(20)),
    }
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
