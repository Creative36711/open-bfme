#!/usr/bin/env python3
"""Transfer exact BFME2/RotWK BIG archives through size-limited storage.

The OpenBFME retail mount requires the original BIG files byte-for-byte.  This
utility packs every ``*.big`` below a BFME2 and a RotWK installation into a
small number of uncompressed volumes.  Each volume is at most 95 MiB by
default, so it can pass through Arena's Google Drive connector (100 MiB per
file).  The receiver streams the volumes straight into the destination and
checks SHA-256 + MD5 for every archive and SHA-256 for every volume.

No game data is committed to Git.  The transfer is just byte splitting: BIG
entry ordering, duplicate names, header quirks, gaps, tails and archive MD5s
are preserved exactly.

Pack on the machine that has the real installations::

    python retail_big_transfer.py pack \
      --bfme2 "D:/Games/BFME2" \
      --rotwk "D:/Games/RotWK" \
      --out "D:/openbfme-retail-transfer"

Upload ``manifest.json`` and every ``retail-bigs.part-*.bin`` from that output
folder to one Google Drive folder.  On the receiving machine::

    python retail_big_transfer.py unpack \
      --manifest /path/to/manifest.json \
      --out /path/to/.retail \
      --delete-parts

The resulting folders are ``<out>/BFME2`` and ``<out>/RotWK``.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import BinaryIO, Iterable

SCHEMA = "openbfme.retail-big-transfer"
SCHEMA_VERSION = 1
DEFAULT_PART_MIB = 95
PART_PREFIX = "retail-bigs.part-"
PART_SUFFIX = ".bin"
MANIFEST_NAME = "manifest.json"
CHECKSUMS_NAME = "SHA256SUMS.txt"
IO_CHUNK = 8 * 1024 * 1024
LFS_POINTER_PREFIX = b"version https://git-lfs.github.com/spec/v1"
BIG_SIGNATURES = (b"BIGF", b"BIG4")


class TransferError(RuntimeError):
    """A safe, user-facing transfer failure."""


def digest_file(path: Path, chunk_size: int = IO_CHUNK) -> tuple[str, str, int]:
    md5 = hashlib.md5()
    sha256 = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(chunk_size)
            if not chunk:
                break
            size += len(chunk)
            md5.update(chunk)
            sha256.update(chunk)
    return md5.hexdigest(), sha256.hexdigest(), size


def ensure_real_big(path: Path) -> None:
    with path.open("rb") as stream:
        head = stream.read(max(64, len(LFS_POINTER_PREFIX)))
    if head.startswith(LFS_POINTER_PREFIX):
        raise TransferError(f"{path} is a Git LFS pointer, not a BIG archive")
    if head[:4] not in BIG_SIGNATURES:
        raise TransferError(
            f"{path} is not a SAGE BIG archive: expected BIGF/BIG4, got {head[:4]!r}"
        )


def collect_big_files(label: str, root: Path) -> list[tuple[str, Path]]:
    if not root.is_dir():
        raise TransferError(f"{label} folder does not exist: {root}")
    found: list[tuple[str, Path]] = []
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.casefold() == ".big":
            ensure_real_big(path)
            rel = path.relative_to(root).as_posix()
            found.append((f"{label}/{rel}", path))
    found.sort(key=lambda item: item[0].casefold())
    if not found:
        raise TransferError(f"no *.big archives found below {root}")
    return found


@dataclass
class PartRecord:
    name: str
    size: int
    sha256: str


class VolumeWriter:
    def __init__(self, output: Path, part_size: int) -> None:
        if part_size <= 0:
            raise TransferError("part size must be positive")
        self.output = output
        self.part_size = part_size
        self.parts: list[PartRecord] = []
        self.total = 0
        self._stream: BinaryIO | None = None
        self._path: Path | None = None
        self._name = ""
        self._size = 0
        self._sha256 = hashlib.sha256()

    def _open(self) -> None:
        index = len(self.parts) + 1
        self._name = f"{PART_PREFIX}{index:04d}{PART_SUFFIX}"
        self._path = self.output / self._name
        self._stream = self._path.open("xb")
        self._size = 0
        self._sha256 = hashlib.sha256()

    def _close(self) -> None:
        if self._stream is None:
            return
        self._stream.flush()
        os.fsync(self._stream.fileno())
        self._stream.close()
        self.parts.append(PartRecord(self._name, self._size, self._sha256.hexdigest()))
        self._stream = None
        self._path = None

    def write(self, data: bytes) -> None:
        view = memoryview(data)
        while view:
            if self._stream is None:
                self._open()
            room = self.part_size - self._size
            take = min(room, len(view))
            block = view[:take]
            assert self._stream is not None
            self._stream.write(block)
            self._sha256.update(block)
            self._size += take
            self.total += take
            view = view[take:]
            if self._size == self.part_size:
                self._close()

    def close(self) -> None:
        self._close()


class VolumeReader:
    def __init__(self, folder: Path, parts: list[dict[str, object]]) -> None:
        self.folder = folder
        self.parts = parts
        self.total = 0
        self._index = -1
        self._stream: BinaryIO | None = None
        self._remaining = 0
        self._sha256 = hashlib.sha256()
        self._record: dict[str, object] | None = None

        seen: set[str] = set()
        for record in parts:
            name = checked_part_name(record.get("name"))
            if name in seen:
                raise TransferError(f"duplicate volume in manifest: {name}")
            seen.add(name)
            size = checked_nonnegative_int(record.get("size"), f"size of {name}")
            checked_hex(record.get("sha256"), 64, f"SHA-256 of {name}")
            path = folder / name
            if not path.is_file():
                raise TransferError(f"missing transfer volume: {path}")
            actual = path.stat().st_size
            if actual != size:
                raise TransferError(
                    f"wrong size for {name}: expected {size}, found {actual}"
                )

    def _finish_current(self) -> None:
        if self._stream is None or self._record is None:
            return
        if self._remaining != 0:
            raise TransferError(f"volume ended early: {self._record['name']}")
        self._stream.close()
        expected = str(self._record["sha256"])
        actual = self._sha256.hexdigest()
        if actual != expected:
            raise TransferError(
                f"SHA-256 mismatch for {self._record['name']}: "
                f"expected {expected}, got {actual}"
            )
        self._stream = None
        self._record = None

    def _open_next(self) -> bool:
        self._finish_current()
        self._index += 1
        if self._index >= len(self.parts):
            return False
        record = self.parts[self._index]
        name = checked_part_name(record.get("name"))
        self._record = record
        self._stream = (self.folder / name).open("rb")
        self._remaining = int(record["size"])
        self._sha256 = hashlib.sha256()
        return True

    def read(self, size: int) -> bytes:
        if size < 0:
            raise TransferError("cannot read a negative byte count")
        output = bytearray()
        while len(output) < size:
            if self._stream is None and not self._open_next():
                raise TransferError(
                    f"transfer stream ended at {self.total} bytes; "
                    f"another {size - len(output)} bytes were expected"
                )
            if self._remaining == 0:
                self._finish_current()
                continue
            take = min(size - len(output), self._remaining, IO_CHUNK)
            assert self._stream is not None
            block = self._stream.read(take)
            if len(block) != take:
                name = self._record["name"] if self._record else "?"
                raise TransferError(f"volume ended early while reading {name}")
            output.extend(block)
            self._sha256.update(block)
            self._remaining -= len(block)
            self.total += len(block)
        return bytes(output)

    def finish(self) -> None:
        self._finish_current()
        if self._index + 1 < len(self.parts):
            # No bytes from the remaining declared volumes were consumed.
            name = self.parts[self._index + 1]["name"]
            raise TransferError(f"manifest has unused trailing volume: {name}")


def checked_nonnegative_int(value: object, description: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise TransferError(f"invalid {description}: {value!r}")
    return value


def checked_hex(value: object, length: int, description: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(rf"[0-9a-f]{{{length}}}", value):
        raise TransferError(f"invalid {description}: {value!r}")
    return value


def checked_part_name(value: object) -> str:
    if not isinstance(value, str) or not re.fullmatch(
        rf"{re.escape(PART_PREFIX)}[0-9]{{4,}}{re.escape(PART_SUFFIX)}", value
    ):
        raise TransferError(f"unsafe or invalid volume name: {value!r}")
    return value


def checked_archive_path(value: object) -> PurePosixPath:
    if not isinstance(value, str):
        raise TransferError(f"invalid archive path: {value!r}")
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or ".." in path.parts
        or "." in path.parts
        or len(path.parts) < 2
        or path.parts[0] not in ("BFME2", "RotWK")
        or path.suffix.casefold() != ".big"
        or any("\x00" in part or ":" in part or "\\" in part for part in path.parts)
    ):
        raise TransferError(f"unsafe archive path in manifest: {value!r}")
    return path


def prepare_empty_output(output: Path) -> None:
    if output.exists():
        if not output.is_dir():
            raise TransferError(f"output is not a folder: {output}")
        if any(output.iterdir()):
            raise TransferError(
                f"output folder is not empty: {output}\n"
                "Choose a new folder or empty it yourself after checking its contents."
            )
    else:
        output.mkdir(parents=True)


def write_json_atomic(path: Path, value: object) -> None:
    temporary = path.with_name(path.name + ".tmp")
    text = json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    with temporary.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(text)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def pack(bfme2: Path, rotwk: Path, output: Path, part_mib: int) -> None:
    part_size = part_mib * 1024 * 1024
    if part_size > 100 * 1024 * 1024:
        raise TransferError(
            "part size exceeds 100 MiB; use 95 MiB for the Google Drive connector"
        )
    prepare_empty_output(output)

    sources = collect_big_files("BFME2", bfme2) + collect_big_files("RotWK", rotwk)
    names: set[str] = set()
    for logical, _ in sources:
        key = logical.casefold()
        if key in names:
            raise TransferError(f"case-insensitive duplicate archive path: {logical}")
        names.add(key)

    writer = VolumeWriter(output, part_size)
    files: list[dict[str, object]] = []
    total_source_size = sum(path.stat().st_size for _, path in sources)
    print(
        f"Packing {len(sources)} BIG archives, "
        f"{total_source_size / (1024 ** 3):.2f} GiB, into {part_mib} MiB volumes"
    )

    for index, (logical, source) in enumerate(sources, 1):
        expected_size = source.stat().st_size
        offset = writer.total
        md5 = hashlib.md5()
        sha256 = hashlib.sha256()
        written = 0
        with source.open("rb") as stream:
            while True:
                chunk = stream.read(IO_CHUNK)
                if not chunk:
                    break
                writer.write(chunk)
                md5.update(chunk)
                sha256.update(chunk)
                written += len(chunk)
        if written != expected_size:
            raise TransferError(
                f"{source} changed while it was being packed: "
                f"expected {expected_size} bytes, read {written}"
            )
        files.append(
            {
                "path": logical,
                "offset": offset,
                "size": written,
                "md5": md5.hexdigest(),
                "sha256": sha256.hexdigest(),
            }
        )
        print(f"[{index:3d}/{len(sources)}] {logical} ({written / (1024 ** 2):.1f} MiB)")
    writer.close()

    manifest = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "partSize": part_size,
        "totalSize": writer.total,
        "fileCount": len(files),
        "files": files,
        "parts": [record.__dict__ for record in writer.parts],
    }
    manifest_path = output / MANIFEST_NAME
    write_json_atomic(manifest_path, manifest)

    lines = []
    manifest_sha = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    lines.append(f"{manifest_sha}  {MANIFEST_NAME}")
    lines.extend(f"{part.sha256}  {part.name}" for part in writer.parts)
    (output / CHECKSUMS_NAME).write_text("\n".join(lines) + "\n", encoding="ascii")

    print(
        f"Done: {len(writer.parts)} volumes in {output}\n"
        f"Upload {MANIFEST_NAME} and all {PART_PREFIX}*{PART_SUFFIX} files.\n"
        f"{CHECKSUMS_NAME} is optional but useful for manual verification."
    )


def load_manifest(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise TransferError(f"cannot read manifest {path}: {error}") from error
    if not isinstance(value, dict):
        raise TransferError("manifest root must be an object")
    if value.get("schema") != SCHEMA or value.get("schemaVersion") != SCHEMA_VERSION:
        raise TransferError(
            f"unsupported manifest: {value.get('schema')!r} "
            f"version {value.get('schemaVersion')!r}"
        )
    if not isinstance(value.get("files"), list) or not isinstance(value.get("parts"), list):
        raise TransferError("manifest must contain files[] and parts[]")
    return value


def existing_matches(path: Path, expected_size: int, md5: str, sha256: str) -> bool:
    if not path.is_file() or path.stat().st_size != expected_size:
        return False
    actual_md5, actual_sha256, actual_size = digest_file(path)
    return (
        actual_size == expected_size
        and actual_md5 == md5
        and actual_sha256 == sha256
    )


def unpack(manifest_path: Path, output: Path, force: bool, delete_parts: bool) -> None:
    manifest = load_manifest(manifest_path)
    files = manifest["files"]
    parts = manifest["parts"]
    assert isinstance(files, list)
    assert isinstance(parts, list)

    expected_count = checked_nonnegative_int(manifest.get("fileCount"), "file count")
    expected_total = checked_nonnegative_int(manifest.get("totalSize"), "total size")
    if expected_count != len(files):
        raise TransferError(
            f"manifest file count is {expected_count}, but files[] has {len(files)} entries"
        )
    if not parts and expected_total:
        raise TransferError("manifest declares data but has no volumes")

    output.mkdir(parents=True, exist_ok=True)
    reader = VolumeReader(manifest_path.parent, parts)
    seen: set[str] = set()
    expected_offset = 0

    print(
        f"Restoring {len(files)} BIG archives, "
        f"{expected_total / (1024 ** 3):.2f} GiB, from {len(parts)} volumes"
    )

    for index, record in enumerate(files, 1):
        if not isinstance(record, dict):
            raise TransferError(f"invalid files[{index - 1}] record")
        logical = checked_archive_path(record.get("path"))
        key = str(logical).casefold()
        if key in seen:
            raise TransferError(f"case-insensitive duplicate archive path: {logical}")
        seen.add(key)

        offset = checked_nonnegative_int(record.get("offset"), f"offset of {logical}")
        size = checked_nonnegative_int(record.get("size"), f"size of {logical}")
        expected_md5 = checked_hex(record.get("md5"), 32, f"MD5 of {logical}")
        expected_sha256 = checked_hex(
            record.get("sha256"), 64, f"SHA-256 of {logical}"
        )
        if offset != expected_offset:
            raise TransferError(
                f"non-contiguous stream at {logical}: expected offset "
                f"{expected_offset}, got {offset}"
            )

        target = output.joinpath(*logical.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        already_valid = existing_matches(
            target, size, expected_md5, expected_sha256
        )
        if target.exists() and not already_valid and not force:
            raise TransferError(
                f"destination exists but does not match the transfer: {target}\n"
                "Use a clean output folder, or pass --force after checking the path."
            )

        temporary = target.with_name(target.name + ".transfer-tmp")
        if temporary.exists():
            temporary.unlink()
        destination: BinaryIO | None = None
        if not already_valid:
            destination = temporary.open("xb")

        md5 = hashlib.md5()
        sha256 = hashlib.sha256()
        remaining = size
        try:
            while remaining:
                block = reader.read(min(remaining, IO_CHUNK))
                md5.update(block)
                sha256.update(block)
                if destination is not None:
                    destination.write(block)
                remaining -= len(block)
            if destination is not None:
                destination.flush()
                os.fsync(destination.fileno())
                destination.close()
                destination = None
        except BaseException:
            if destination is not None:
                destination.close()
            temporary.unlink(missing_ok=True)
            raise

        actual_md5 = md5.hexdigest()
        actual_sha256 = sha256.hexdigest()
        if actual_md5 != expected_md5 or actual_sha256 != expected_sha256:
            temporary.unlink(missing_ok=True)
            raise TransferError(
                f"archive checksum mismatch for {logical}:\n"
                f"  MD5 expected {expected_md5}, got {actual_md5}\n"
                f"  SHA-256 expected {expected_sha256}, got {actual_sha256}"
            )
        if not already_valid:
            ensure_real_big(temporary)
            os.replace(temporary, target)
        expected_offset += size
        action = "already valid" if already_valid else "restored"
        print(f"[{index:3d}/{len(files)}] {logical} ({action})")

    reader.finish()
    if expected_offset != expected_total or reader.total != expected_total:
        raise TransferError(
            f"total size mismatch: manifest {expected_total}, "
            f"file table {expected_offset}, stream {reader.total}"
        )

    if delete_parts:
        for record in parts:
            name = checked_part_name(record.get("name"))
            (manifest_path.parent / name).unlink()
        checksum_path = manifest_path.parent / CHECKSUMS_NAME
        checksum_path.unlink(missing_ok=True)
        print("Transfer volumes deleted after successful restoration")

    print(
        "Done. Use these install folders:\n"
        f"  BFME2_INSTALL={output / 'BFME2'}\n"
        f"  ROTWK_INSTALL={output / 'RotWK'}"
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Transfer exact BFME2/RotWK BIG archives in <=100 MiB volumes"
    )
    sub = parser.add_subparsers(dest="command", required=True)

    pack_parser = sub.add_parser("pack", help="pack real installations into volumes")
    pack_parser.add_argument("--bfme2", type=Path, required=True, help="BFME2 1.06 folder")
    pack_parser.add_argument("--rotwk", type=Path, required=True, help="RotWK 2.01 folder")
    pack_parser.add_argument("--out", type=Path, required=True, help="new/empty output folder")
    pack_parser.add_argument(
        "--part-mib",
        type=int,
        default=DEFAULT_PART_MIB,
        help=f"volume size in MiB (default: {DEFAULT_PART_MIB}, maximum: 100)",
    )

    unpack_parser = sub.add_parser("unpack", help="restore exact BIG files from volumes")
    unpack_parser.add_argument("--manifest", type=Path, required=True)
    unpack_parser.add_argument("--out", type=Path, required=True, help="destination root")
    unpack_parser.add_argument(
        "--force", action="store_true", help="replace mismatching destination files"
    )
    unpack_parser.add_argument(
        "--delete-parts",
        action="store_true",
        help="delete volume files after every archive and checksum passed",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "pack":
            pack(args.bfme2, args.rotwk, args.out, args.part_mib)
        elif args.command == "unpack":
            unpack(args.manifest, args.out, args.force, args.delete_parts)
        else:
            parser.error(f"unknown command: {args.command}")
        return 0
    except TransferError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())

# End-of-file sentinel: keep trailing lines so file-transfer UIs cannot drop the
# executable statement while trimming the final line.
