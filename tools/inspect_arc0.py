#!/usr/bin/env python3
"""Read Level-5 ARC0 indexes without extracting archive payloads.

The reader follows the ARC0 layout used by Kuriimu2's Level-5 plugin. It
parses directory/file/name tables and emits metadata only; file data is never
copied or written by this tool.
"""

from __future__ import annotations

import argparse
import io
import json
import struct
import sys
from collections import Counter
from pathlib import Path
from typing import BinaryIO, Callable


class Arc0Error(RuntimeError):
    """Raised when an ARC0 header or table is inconsistent."""


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_exact(stream: BinaryIO, size: int) -> bytes:
    data = stream.read(size)
    if len(data) != size:
        raise Arc0Error(f"short read: expected {size} bytes, got {len(data)}")
    return data


def decode_level5_lz10(stream: io.BytesIO, size: int) -> bytes:
    output = bytearray()
    circular = bytearray(0x1000)
    write_pos = 0
    flags = 0
    mask = 1

    while len(output) < size:
        if mask == 1:
            flags = stream.read(1)
            if not flags:
                raise Arc0Error("truncated Level-5 LZ10 flags")
            flags = flags[0]
            mask = 0x80
        else:
            mask >>= 1

        if flags & mask:
            pair = read_exact(stream, 2)
            length = (pair[0] >> 4) + 3
            displacement = ((pair[0] & 0x0F) << 8 | pair[1]) + 1
            if displacement > len(output):
                raise Arc0Error(
                    f"invalid LZ10 displacement {displacement} at output {len(output)}"
                )
            for _ in range(length):
                value = circular[(write_pos - displacement) & 0x0FFF]
                output.append(value)
                circular[write_pos] = value
                write_pos = (write_pos + 1) & 0x0FFF
                if len(output) == size:
                    break
        else:
            value = stream.read(1)
            if not value:
                raise Arc0Error("truncated Level-5 LZ10 literal")
            output.append(value[0])
            circular[write_pos] = value[0]
            write_pos = (write_pos + 1) & 0x0FFF

    return bytes(output)


def decode_level5_huffman(stream: io.BytesIO, size: int, bit_depth: int) -> bytes:
    tree_size_raw = stream.read(1)
    tree_root_raw = stream.read(1)
    if not tree_size_raw or not tree_root_raw:
        raise Arc0Error("truncated Level-5 Huffman tree header")

    tree_size = tree_size_raw[0]
    tree_root = tree_root_raw[0]
    tree = read_exact(stream, tree_size * 2)
    symbol_count = size * 8 // bit_depth
    result = bytearray()
    code = 0
    next_index = 0
    position = tree_root

    bit_index = 0
    while len(result) < symbol_count:
        if bit_index % 32 == 0:
            code = u32(read_exact(stream, 4), 0)

        next_index += ((position & 0x3F) << 1) + 2
        direction = 2 if ((code >> (31 - (bit_index % 32))) & 1) == 0 else 1
        leaf = (((position >> 5) >> direction) & 1) != 0
        tree_index = next_index - direction
        if tree_index < 0 or tree_index >= len(tree):
            raise Arc0Error("invalid Level-5 Huffman tree pointer")
        position = tree[tree_index]
        if leaf:
            result.append(position)
            position = tree_root
            next_index = 0
        bit_index += 1

    if bit_depth == 8:
        return bytes(result[:size])

    combined = bytearray()
    for index in range(size):
        low = result[index * 2]
        high = result[index * 2 + 1]
        combined.append(low | (high << 4))
    return bytes(combined)


def decode_level5_rle(stream: io.BytesIO, size: int) -> bytes:
    output = bytearray()
    while len(output) < size:
        flag_raw = stream.read(1)
        if not flag_raw:
            raise Arc0Error("truncated Level-5 RLE stream")
        flag = flag_raw[0]
        if flag & 0x80:
            repetitions = (flag & 0x7F) + 3
            value = stream.read(1)
            if not value:
                raise Arc0Error("truncated Level-5 RLE repeat")
            output.extend(value * repetitions)
        else:
            length = flag + 1
            output.extend(read_exact(stream, length))
    if len(output) < size:
        raise Arc0Error("short Level-5 RLE output")
    return bytes(output[:size])


def decompress_level5(blob: bytes, label: str) -> tuple[bytes, dict[str, int | str]]:
    if len(blob) < 4:
        raise Arc0Error(f"{label}: missing compression header")
    method_size = u32(blob, 0)
    size = method_size >> 3
    method = method_size & 0x7
    payload = io.BytesIO(blob[4:])

    if method == 0:
        output = read_exact(payload, size)
        method_name = "none"
    elif method == 1:
        output = decode_level5_lz10(payload, size)
        method_name = "lz10"
    elif method == 2:
        output = decode_level5_huffman(payload, size, 4)
        method_name = "huffman4"
    elif method == 3:
        output = decode_level5_huffman(payload, size, 8)
        method_name = "huffman8"
    elif method == 4:
        output = decode_level5_rle(payload, size)
        method_name = "rle"
    elif method == 5:
        import zlib

        output = zlib.decompress(payload.read())[:size]
        method_name = "zlib"
    else:
        raise Arc0Error(f"{label}: unsupported Level-5 compression method {method}")

    if len(output) != size:
        raise Arc0Error(f"{label}: expected {size} bytes, got {len(output)}")
    return output, {
        "method": method_name,
        "method_id": method,
        "compressed_bytes": len(blob),
        "decompressed_bytes": size,
    }


def read_c_string(data: bytes, offset: int, label: str) -> str:
    if offset < 0 or offset >= len(data):
        raise Arc0Error(f"{label}: string offset {offset} is outside name table")
    end = data.find(b"\0", offset)
    if end < 0:
        raise Arc0Error(f"{label}: unterminated string at {offset}")
    return data[offset:end].decode("shift_jis", errors="replace")


def parse_arc0(stream: BinaryIO) -> dict:
    stream.seek(0, 2)
    archive_size = stream.tell()
    stream.seek(0)
    header = read_exact(stream, 0x48)
    if header[:4] != b"ARC0":
        raise Arc0Error(f"invalid magic: {header[:4]!r}")

    fields = {
        "magic": header[:4].decode("ascii"),
        "directory_entries_offset": u32(header, 0x04),
        "directory_hash_offset": u32(header, 0x08),
        "file_entries_offset": u32(header, 0x0C),
        "name_offset": u32(header, 0x10),
        "data_offset": u32(header, 0x14),
        "directory_entries_count": i16(header, 0x18),
        "directory_hash_count": i16(header, 0x1A),
        "file_entries_count": u32(header, 0x1C),
        "table_chunk_size": u32(header, 0x20),
        "zero1": u32(header, 0x24),
        "unknown2": u32(header, 0x28),
        "unknown3": u32(header, 0x2C),
        "unknown4": u32(header, 0x30),
        "unknown5": u32(header, 0x34),
        "directory_count": u32(header, 0x38),
        "file_count": u32(header, 0x3C),
        "unknown7": u32(header, 0x40),
        "zero2": u32(header, 0x44),
    }

    for name in (
        "directory_entries_offset",
        "directory_hash_offset",
        "file_entries_offset",
        "name_offset",
        "data_offset",
    ):
        if fields[name] < 0x48 or fields[name] > archive_size:
            raise Arc0Error(f"{name} is outside archive: {fields[name]:#x}")

    def chunk(start_name: str, end_name: str, label: str) -> tuple[bytes, dict]:
        start = fields[start_name]
        end = fields[end_name]
        if end < start:
            raise Arc0Error(f"{label}: descending table offsets")
        stream.seek(start)
        return decompress_level5(read_exact(stream, end - start), label)

    directory_data, directory_compression = chunk(
        "directory_entries_offset", "directory_hash_offset", "directory entries"
    )
    hash_data, hash_compression = chunk(
        "directory_hash_offset", "file_entries_offset", "directory hashes"
    )
    file_data, file_compression = chunk(
        "file_entries_offset", "name_offset", "file entries"
    )
    stream.seek(fields["name_offset"])
    name_data, name_compression = decompress_level5(
        read_exact(stream, fields["data_offset"] - fields["name_offset"]), "names"
    )

    directory_count = fields["directory_entries_count"]
    file_count = fields["file_entries_count"]
    if len(directory_data) < directory_count * 20:
        raise Arc0Error("directory table is smaller than its declared entry count")
    if len(hash_data) < fields["directory_hash_count"] * 4:
        raise Arc0Error("directory hash table is smaller than its declared count")
    if len(file_data) < file_count * 16:
        raise Arc0Error("file table is smaller than its declared entry count")

    directories = []
    for index in range(directory_count):
        offset = index * 20
        directories.append(
            {
                "crc32": u32(directory_data, offset),
                "first_directory_index": u16(directory_data, offset + 4),
                "directory_count": i16(directory_data, offset + 6),
                "first_file_index": u16(directory_data, offset + 8),
                "file_count": i16(directory_data, offset + 10),
                "file_name_start_offset": u32(directory_data, offset + 12),
                "directory_name_start_offset": u32(directory_data, offset + 16),
            }
        )

    files = []
    for index in range(file_count):
        offset = index * 16
        files.append(
            {
                "crc32": u32(file_data, offset),
                "name_offset_in_folder": u32(file_data, offset + 4),
                "file_offset": u32(file_data, offset + 8),
                "file_size": u32(file_data, offset + 12),
            }
        )

    manifest_files = []
    for directory in directories:
        directory_name = read_c_string(
            name_data,
            directory["directory_name_start_offset"],
            "directory name",
        )
        first_file = directory["first_file_index"]
        last_file = first_file + directory["file_count"]
        if last_file > len(files):
            raise Arc0Error("directory file range exceeds file table")
        for file_entry in files[first_file:last_file]:
            file_name = read_c_string(
                name_data,
                directory["file_name_start_offset"]
                + file_entry["name_offset_in_folder"],
                "file name",
            )
            absolute_offset = fields["data_offset"] + file_entry["file_offset"]
            absolute_end = absolute_offset + file_entry["file_size"]
            if absolute_end > archive_size:
                raise Arc0Error(f"file exceeds archive: {directory_name}{file_name}")
            manifest_files.append(
                {
                    "path": f"{directory_name}{file_name}",
                    "offset": absolute_offset,
                    "size": file_entry["file_size"],
                    "crc32": f"{file_entry['crc32']:08x}",
                }
            )

    return {
        "format": "ARC0",
        "archive_size": archive_size,
        "header": fields,
        "compression": {
            "directory_entries": directory_compression,
            "directory_hashes": hash_compression,
            "file_entries": file_compression,
            "names": name_compression,
        },
        "directory_count": len(directories),
        "file_count": len(manifest_files),
        "payload_bytes": sum(item["size"] for item in manifest_files),
        "files": manifest_files,
    }


def open_source(args: argparse.Namespace) -> tuple[BinaryIO, Callable[[], None]]:
    if args.archive:
        handle = Path(args.archive).open("rb")
        return handle, handle.close

    if not args.rom or not args.romfs_path:
        raise Arc0Error("provide --archive or both --rom and --romfs-path")
    try:
        from pyctr.crypto import CryptoEngine
        from pyctr.type.cci import CCIReader, CCISection
        from pyctr.type.ncch import NCCHReader
    except ImportError as exc:
        raise Arc0Error("--rom requires pyctr; install tools/requirements-3ds.txt") from exc

    cci = CCIReader(args.rom, load_contents=False)
    ncch = NCCHReader(
        cci.open_raw_section(CCISection.Application),
        crypto=CryptoEngine(setup_b9_keys=False),
        assume_decrypted=True,
    )
    handle = ncch.romfs.open(args.romfs_path)

    def close() -> None:
        handle.close()
        ncch.close()
        cci.close()

    return handle, close


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", help="path to an extracted .fa file")
    parser.add_argument("--rom", help="decrypted CCI/3DS image")
    parser.add_argument("--romfs-path", help="path such as /yw2_a.fa inside the application RomFS")
    parser.add_argument("--json-out", type=Path, help="write the complete metadata manifest locally")
    args = parser.parse_args()

    try:
        stream, close = open_source(args)
        try:
            result = parse_arc0(stream)
        finally:
            close()
    except (Arc0Error, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    extension_counts = Counter(Path(item["path"]).suffix.lower() or "<none>" for item in result["files"])
    print(json.dumps({
        "format": result["format"],
        "archive_size": result["archive_size"],
        "directory_count": result["directory_count"],
        "file_count": result["file_count"],
        "payload_bytes": result["payload_bytes"],
        "compression": {name: value["method"] for name, value in result["compression"].items()},
        "extensions": dict(sorted(extension_counts.items())),
        "json_out": str(args.json_out) if args.json_out else None,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
