#!/usr/bin/env python3
"""Build a private YW1-Mobile host package with YW2 data overlaid.

The tool operates only on user-provided local files. It keeps the YW1 mobile
executable and platform shell, then maps paths from an extracted YW2 ARC0
archive into the data directory used by the APK or IPA. It does not add any
copyrighted package to the repository and it does not sign the result.

The output package must be signed again by its owner. This is an experimental
resource overlay: it cannot make incompatible XQ32 opcodes, databases, models,
or shaders compatible by renaming files. The JSON report is intentionally
metadata-only and is useful for deciding which converters are still needed.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import posixpath
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, NoReturn
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


@dataclass(frozen=True)
class ArchiveFile:
    path: str
    offset: int
    size: int


def fail(message: str) -> "NoReturn":
    raise SystemExit(f"error: {message}")


def load_manifest(path: Path) -> list[ArchiveFile]:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        fail(f"cannot read ARC0 manifest {path}: {exc}")
    except json.JSONDecodeError as exc:
        fail(f"invalid ARC0 manifest {path}: {exc}")

    files = raw.get("files")
    if not isinstance(files, list):
        fail("ARC0 manifest has no files list")
    result: list[ArchiveFile] = []
    seen: set[str] = set()
    for item in files:
        if not isinstance(item, dict):
            fail("ARC0 manifest contains a non-object file entry")
        path_value = item.get("path")
        offset = item.get("offset")
        size = item.get("size")
        if not isinstance(path_value, str) or not isinstance(offset, int) or not isinstance(size, int):
            fail("ARC0 file entry must contain string path and integer offset/size")
        normalized = posixpath.normpath(path_value)
        if normalized != path_value or normalized.startswith("../") or normalized.startswith("/"):
            fail(f"unsafe ARC0 path: {path_value!r}")
        if normalized in seen:
            fail(f"duplicate ARC0 path: {normalized}")
        if offset < 0 or size < 0:
            fail(f"negative ARC0 range for {normalized}")
        seen.add(normalized)
        result.append(ArchiveFile(normalized, offset, size))
    return result


def package_root(kind: str) -> str:
    if kind == "apk":
        return "assets/"
    return "Payload/yw1_rom.app/"


def package_path(path: str, kind: str) -> str:
    # YW2's ARC0 stores data/* at the content root, while seq/* and other
    # resources live under data/ in YW1 Mobile packages.
    root = package_root(kind)
    if kind == "ipa":
        # The iOS bundle keeps the same extra data/ component for both
        # `seq/...` and ARC0 paths that already begin with `data/...`.
        return root + "data/" + path
    return root + path if path.startswith("data/") else root + "data/" + path


def selected(path: str, includes: Iterable[str], excludes: Iterable[str]) -> bool:
    include_patterns = tuple(includes)
    exclude_patterns = tuple(excludes)
    if include_patterns and not any(fnmatch.fnmatchcase(path, pattern) for pattern in include_patterns):
        return False
    return not any(fnmatch.fnmatchcase(path, pattern) for pattern in exclude_patterns)


def package_kind(path: Path, requested: str) -> str:
    if requested != "auto":
        return requested
    suffix = path.suffix.lower()
    if suffix == ".apk":
        return "apk"
    if suffix == ".ipa":
        return "ipa"
    fail("use --kind apk or --kind ipa when the output has no .apk/.ipa suffix")


def package_entries(package: ZipFile, kind: str) -> dict[str, ZipInfo]:
    prefix = package_root(kind)
    entries: dict[str, ZipInfo] = {}
    for info in package.infolist():
        if info.filename.startswith(prefix):
            entries[info.filename] = info
    return entries


def copy_info(info: ZipInfo) -> ZipInfo:
    # ZipInfo is mutable and cannot be reused after changing its filename.
    clone = ZipInfo(info.filename, info.date_time)
    clone.compress_type = ZIP_DEFLATED
    clone.comment = info.comment
    clone.extra = info.extra
    clone.internal_attr = info.internal_attr
    clone.external_attr = info.external_attr
    clone.create_system = info.create_system
    clone.create_version = info.create_version
    clone.extract_version = info.extract_version
    clone.flag_bits = info.flag_bits
    return clone


def write_overlay(
    base_package: Path,
    arc0: Path,
    manifest: list[ArchiveFile],
    output: Path,
    kind: str,
    includes: Iterable[str],
    excludes: Iterable[str],
    dry_run: bool,
) -> dict:
    if base_package.resolve() == output.resolve():
        fail("output must differ from the original YW1 package")
    try:
        arc0_size = arc0.stat().st_size
        if not base_package.is_file():
            fail(f"base package does not exist: {base_package}")
    except OSError as exc:
        fail(str(exc))

    selected_files = [item for item in manifest if selected(item.path, includes, excludes)]
    invalid = [item.path for item in selected_files if item.offset + item.size > arc0_size]
    if invalid:
        fail(f"ARC0 ranges exceed {arc0}: {invalid[0]}")

    with ZipFile(base_package, "r") as base:
        base_entries = package_entries(base, kind)
        mapped = {item.path: package_path(item.path, kind) for item in selected_files}
        collisions = sum(target in base_entries for target in mapped.values())
        report = {
            "base_package": str(base_package),
            "arc0": str(arc0),
            "manifest_files": len(manifest),
            "selected_files": len(selected_files),
            "mapped_paths": len(mapped),
            "replaced_yw1_entries": collisions,
            "new_yw2_entries": len(selected_files) - collisions,
            "kind": kind,
            "signed": False,
            "dry_run": dry_run,
        }
        if dry_run:
            return report

        output.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary_name = tempfile.mkstemp(prefix=output.name + ".", suffix=".tmp", dir=output.parent)
        os.close(fd)
        temporary = Path(temporary_name)
        try:
            with ZipFile(temporary, "w", compression=ZIP_DEFLATED, allowZip64=True) as destination:
                replacements = set(mapped.values())
                for info in base.infolist():
                    if info.filename in replacements:
                        continue
                    destination.writestr(copy_info(info), base.read(info))
                with arc0.open("rb") as source:
                    for item in selected_files:
                        source.seek(item.offset)
                        payload = source.read(item.size)
                        if len(payload) != item.size:
                            fail(f"short read in ARC0 payload: {item.path}")
                        destination.writestr(package_path(item.path, kind), payload)
            os.replace(temporary, output)
        finally:
            if temporary.exists():
                temporary.unlink()
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--yw1", type=Path, required=True, help="local YW1 APK or IPA")
    parser.add_argument("--yw2-arc0", type=Path, required=True, help="extracted YW2 ARC0, e.g. yw2_a.fa")
    parser.add_argument("--yw2-manifest", type=Path, required=True, help="inspect_arc0.py JSON manifest")
    parser.add_argument("--output", type=Path, required=True, help="new private APK/IPA path")
    parser.add_argument("--kind", choices=("auto", "apk", "ipa"), default="auto")
    parser.add_argument("--include", action="append", default=[], help="ARC0 glob to include; repeatable")
    parser.add_argument("--exclude", action="append", default=[], help="ARC0 glob to exclude; repeatable")
    parser.add_argument("--dry-run", action="store_true", help="report only; do not create output")
    parser.add_argument("--report", type=Path, help="write metadata-only JSON report")
    args = parser.parse_args()

    kind = package_kind(args.yw1, args.kind)
    if not args.yw1.is_file():
        fail(f"YW1 package does not exist: {args.yw1}")
    if not args.yw2_arc0.is_file():
        fail(f"YW2 ARC0 does not exist: {args.yw2_arc0}")
    manifest = load_manifest(args.yw2_manifest)
    report = write_overlay(
        args.yw1,
        args.yw2_arc0,
        manifest,
        args.output,
        kind,
        args.include,
        args.exclude,
        args.dry_run,
    )
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    main()
