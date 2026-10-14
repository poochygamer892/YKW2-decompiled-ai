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
import struct
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, NoReturn
from zipfile import ZipFile, ZipInfo


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


def patch_android_manifest(payload: bytes) -> bytes:
    # Keep every replacement the same byte length so the binary AXML string
    # pool and resource offsets remain valid. The package ID is changed to
    # make Android distinguish this private build from the YW1 host.
    replacements = {
        # Full-width ２ keeps the original 14 UTF-16 characters and 16 UTF-8
        # bytes, so the binary XML string-pool offsets remain valid.
        b"Yo-kai watch \xef\xbc\x91": b"Yo-kai Watch \xef\xbc\x92",
        b"jp.co.level5.yws4.deploygateprovider": b"jp.co.level5.yws2.deploygateprovider",
        b"jp.co.level5.yws4": b"jp.co.level5.yws2",
        b"jp.co.level5.yws1": b"jp.co.level5.yws2",
    }
    for old, new in replacements.items():
        if len(old) != len(new):
            fail("internal Android identity replacement length mismatch")
        payload = payload.replace(old, new)
    if b"Yo-kai Watch \xef\xbc\x92" not in payload or b"jp.co.level5.yws2" not in payload:
        fail("Android manifest identity was not found")
    return payload


def replace_android_utf8_string(payload: bytes, old: str, new: str) -> bytes:
    old_bytes = old.encode("utf-8")
    new_bytes = new.encode("utf-8")
    if len(old_bytes) != len(new_bytes):
        fail(f"Android resource replacement changes byte length: {old!r}")
    positions = []
    start = 0
    while True:
        position = payload.find(old_bytes, start)
        if position < 0:
            break
        positions.append(position)
        start = position + 1
    if len(positions) != 1:
        fail(f"expected one Android resource string {old!r}, found {len(positions)}")
    position = positions[0]
    old_utf16_length = len(old.encode("utf-16-le")) // 2
    old_utf8_length = len(old_bytes)
    if position < 2 or payload[position - 2:position] != bytes((old_utf16_length, old_utf8_length)):
        fail(f"invalid Android UTF-8 string-pool entry for {old!r}")
    new_utf16_length = len(new.encode("utf-16-le")) // 2
    new_utf8_length = len(new_bytes)
    if new_utf16_length > 0x7f or new_utf8_length > 0x7f:
        fail(f"Android resource replacement needs a long string length: {new!r}")
    patched = bytearray(payload)
    patched[position - 2:position] = bytes((new_utf16_length, new_utf8_length))
    patched[position:position + len(old_bytes)] = new_bytes
    return bytes(patched)


def patch_android_resources(payload: bytes) -> bytes:
    # Android keeps resource package names in UTF-16 and labels in the
    # resource string pool. Keep all byte spans unchanged so table offsets
    # remain valid while making PackageManager see a coherent YW2 identity.
    old_package = "jp.co.level5.yws4".encode("utf-16-le")
    new_package = "jp.co.level5.yws2".encode("utf-16-le")
    if payload.count(old_package) != 1:
        fail("Android resource package name was not found exactly once")
    payload = payload.replace(old_package, new_package)
    payload = replace_android_utf8_string(payload, "jp.co.level5.yws1", "jp.co.level5.yws2")
    payload = replace_android_utf8_string(payload, "妖怪ウォッチ１ スマホ", "Yo-kai Watch 2 Smartphone      ")
    payload = replace_android_utf8_string(payload, "妖怪ｳｫｯﾁ1", "Yo-kai Watch 2 App!")
    return payload


def patch_ios_info_plist(payload: bytes) -> bytes:
    replacements = {
        "妖怪ｳｫｯﾁ1".encode("utf-8"): b"Yo-kai Watch 2 Smartphone",
        b"jp.co.level5.yws1": b"jp.co.level5.yws2",
    }
    for old, new in replacements.items():
        if old not in payload:
            fail(f"iOS identity value was not found: {old!r}")
        payload = payload.replace(old, new)
    return payload


def dos_datetime(info: ZipInfo) -> tuple[int, int]:
    year, month, day, hour, minute, second = info.date_time
    year = max(1980, min(year, 2107))
    return ((hour << 11) | (minute << 5) | (second // 2),
            ((year - 1980) << 9) | (month << 5) | day)


def write_classic_entry(destination, central: list[bytes], name: str,
                        payload: bytes, template: ZipInfo | None = None,
                        compress: bool = True) -> None:
    name_bytes = name.encode("utf-8")
    if len(name_bytes) > 0xffff:
        fail(f"ZIP filename is too long: {name}")
    crc = zlib.crc32(payload) & 0xffffffff
    method = 0
    compressed = payload
    if compress and payload:
        compressor = zlib.compressobj(level=6, method=zlib.DEFLATED, wbits=-15)
        candidate = compressor.compress(payload) + compressor.flush()
        if len(candidate) < len(payload):
            method = 8
            compressed = candidate
    compressed_size = len(compressed)
    uncompressed_size = len(payload)
    offset = destination.tell()
    if any(value >= 0x100000000 for value in (offset, compressed_size, uncompressed_size)):
        fail("classic ZIP limit exceeded; split or reduce the overlay")
    mod_time, mod_date = dos_datetime(template) if template else (0, 33)
    flags = 0x800  # UTF-8 names; all current paths are ASCII-compatible.
    local = struct.pack(
        "<I5H3I2H", 0x04034B50, 20, flags, method, mod_time, mod_date,
        crc, compressed_size, uncompressed_size, len(name_bytes), 0,
    )
    destination.write(local)
    destination.write(name_bytes)
    destination.write(compressed)
    external_attr = template.external_attr if template else 0
    central.append(struct.pack(
        "<I6H3I5H2I", 0x02014B50, 20, 20, flags, method, mod_time, mod_date,
        crc, compressed_size, uncompressed_size, len(name_bytes), 0, 0, 0, 0,
        external_attr, offset,
    ) + name_bytes)


def finish_classic_zip(destination, central: list[bytes]) -> None:
    central_offset = destination.tell()
    for entry in central:
        destination.write(entry)
    central_size = destination.tell() - central_offset
    if any(value >= 0x100000000 for value in (central_offset, central_size)):
        fail("classic ZIP central directory limit exceeded")
    if len(central) >= 0xffff:
        fail("classic ZIP entry-count limit exceeded")
    destination.write(struct.pack(
        "<IHHHHIIH", 0x06054B50, 0, 0, len(central), len(central), central_size,
        central_offset, 0,
    ))


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
            "classic_zip": True,
            "identity": "jp.co.level5.yws2 / Yo-kai Watch 2 Smartphone",
            "dry_run": dry_run,
        }
        if dry_run:
            return report

        output.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary_name = tempfile.mkstemp(prefix=output.name + ".", suffix=".tmp", dir=output.parent)
        os.close(fd)
        temporary = Path(temporary_name)
        try:
            with temporary.open("wb") as destination:
                central: list[bytes] = []
                replacements = set(mapped.values())
                for info in base.infolist():
                    if info.filename in replacements:
                        continue
                    payload = base.read(info)
                    if kind == "apk" and info.filename == "AndroidManifest.xml":
                        payload = patch_android_manifest(payload)
                    elif kind == "apk" and info.filename == "resources.arsc":
                        payload = patch_android_resources(payload)
                    elif kind == "ipa" and info.filename == "Payload/yw1_rom.app/Info.plist":
                        payload = patch_ios_info_plist(payload)
                    write_classic_entry(
                        destination, central, info.filename, payload, info,
                        compress=not info.is_dir(),
                    )
                with arc0.open("rb") as source:
                    for item in selected_files:
                        source.seek(item.offset)
                        payload = source.read(item.size)
                        if len(payload) != item.size:
                            fail(f"short read in ARC0 payload: {item.path}")
                        write_classic_entry(
                            destination, central, package_path(item.path, kind), payload,
                        )
                finish_classic_zip(destination, central)
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
