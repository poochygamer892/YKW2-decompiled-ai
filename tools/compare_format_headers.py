#!/usr/bin/env python3
"""Classify ARC0 payload headers and compare shared paths with mobile ZIPs.

Only a small prefix of each selected file is read. No file is extracted or
written to the repository, and the JSON output is intended to stay local.
"""

from __future__ import annotations

import argparse
import json
import posixpath
import sys
from collections import Counter, defaultdict
from pathlib import Path
from zipfile import ZipFile, ZipInfo


def extension(path: str) -> str:
    return Path(path).suffix.lower() or "<none>"


def signature(data: bytes) -> str:
    return data[:4].hex()


def ascii_signature(hex_value: str) -> str:
    raw = bytes.fromhex(hex_value)
    if all(32 <= value < 127 for value in raw):
        return raw.decode("ascii")
    return ""


def load_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def zip_index(path: Path, prefix: str) -> tuple[dict[str, ZipInfo], dict]:
    archive = ZipFile(path)
    index: dict[str, ZipInfo] = {}
    for info in archive.infolist():
        if info.filename.startswith(prefix):
            normalized = posixpath.normpath(info.filename[len(prefix):])
            index[normalized] = info
    return index, {"path": str(path), "entries": len(archive.infolist()), "selected": len(index)}


def read_zip_prefix(archive: ZipFile, info: ZipInfo, size: int = 64) -> bytes:
    with archive.open(info) as stream:
        return stream.read(size)


def add_header_stats(stats: dict, ext: str, data: bytes, file_size: int) -> None:
    bucket = stats.setdefault(ext, {"count": 0, "total_bytes": 0, "signatures": Counter()})
    bucket["count"] += 1
    bucket["total_bytes"] += file_size
    bucket["signatures"][signature(data)] += 1


def normalize_stats(stats: dict) -> dict:
    result = {}
    for ext, value in sorted(stats.items()):
        signatures = []
        for sig, count in value["signatures"].most_common(8):
            signatures.append({
                "first4_hex": sig,
                "first4_ascii": ascii_signature(sig),
                "count": count,
            })
        result[ext] = {
            "count": value["count"],
            "total_bytes": value["total_bytes"],
            "top_first4_signatures": signatures,
        }
    return result


def compare_bucket() -> dict:
    return {
        "count": 0,
        "size_equal": 0,
        "header_equal": 0,
        "size_and_header_equal": 0,
    }


def comparison_bundle() -> dict:
    return {
        "paths": 0,
        "apk": compare_bucket(),
        "ipa": compare_bucket(),
        "apk_vs_ipa": compare_bucket(),
    }


def update_compare(bucket: dict, left_size: int, right_size: int, left_header: bytes, right_header: bytes) -> None:
    bucket["count"] += 1
    same_size = left_size == right_size
    same_header = left_header[:4] == right_header[:4]
    bucket["size_equal"] += int(same_size)
    bucket["header_equal"] += int(same_header)
    bucket["size_and_header_equal"] += int(same_size and same_header)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, help="decrypted 3DS CCI/NCSD image")
    parser.add_argument("--romfs-path", default="/yw2_a.fa")
    parser.add_argument("--arc0-manifest", required=True, type=Path)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--ipa", required=True, type=Path)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    try:
        from pyctr.crypto import CryptoEngine
        from pyctr.type.cci import CCIReader, CCISection
        from pyctr.type.ncch import NCCHReader
    except ImportError as exc:
        print("error: --rom requires pyctr; install tools/requirements-3ds.txt", file=sys.stderr)
        return 2

    manifest = load_manifest(args.arc0_manifest)
    apk_index, apk_info = zip_index(args.apk, "assets/")
    ipa_index, ipa_info = zip_index(args.ipa, "Payload/yw1_rom.app/data/")
    apk = ZipFile(args.apk)
    ipa = ZipFile(args.ipa)

    format_stats: dict = {}
    shared = set(apk_index) & set(ipa_index)
    arc_shared = [item for item in manifest["files"] if item["path"] in shared]
    comparisons: dict[str, dict] = defaultdict(comparison_bundle)

    with CCIReader(args.rom, load_contents=False) as cci:
        ncch = NCCHReader(
            cci.open_raw_section(CCISection.Application),
            crypto=CryptoEngine(setup_b9_keys=False),
            assume_decrypted=True,
        )
        try:
            romfs_file = ncch.romfs.open(args.romfs_path)
            try:
                for item in manifest["files"]:
                    romfs_file.seek(item["offset"])
                    data = romfs_file.read(min(64, item["size"]))
                    add_header_stats(format_stats, extension(item["path"]), data, item["size"])

                for item in arc_shared:
                    path = item["path"]
                    ext = extension(path)
                    romfs_file.seek(item["offset"])
                    arc_header = romfs_file.read(min(64, item["size"]))
                    apk_header = read_zip_prefix(apk, apk_index[path], 64)
                    ipa_header = read_zip_prefix(ipa, ipa_index[path], 64)
                    comparisons[ext]["paths"] += 1
                    update_compare(comparisons[ext]["apk"], item["size"], apk_index[path].file_size, arc_header, apk_header)
                    update_compare(comparisons[ext]["ipa"], item["size"], ipa_index[path].file_size, arc_header, ipa_header)
                    update_compare(comparisons[ext]["apk_vs_ipa"], apk_index[path].file_size, ipa_index[path].file_size, apk_header, ipa_header)
            finally:
                romfs_file.close()
        finally:
            ncch.close()

    report = {
        "comparison": "first 64 bytes and declared sizes; no extraction",
        "arc0": {
            "manifest": str(args.arc0_manifest),
            "romfs_path": args.romfs_path,
            "file_count": manifest["file_count"],
            "format_summary": normalize_stats(format_stats),
        },
        "mobile": {
            "apk": apk_info,
            "ipa": ipa_info,
            "shared_normalized_paths": len(shared),
            "shared_paths_compared_with_3ds": len(arc_shared),
            "comparison_by_extension": {
                ext: value
                for ext, value in sorted(comparisons.items())
            },
        },
    }
    apk.close()
    ipa.close()

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
