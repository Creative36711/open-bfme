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
INGEST_STATE_NAME = ".retail-big-transfer-state.json"
INGEST_STATE_SCHEMA = "openbfme.retail-big-transfer.ingest-state"
INGEST_STATE_VERSION = 1
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


@dataclass(frozen=True)
class PartRecord:
    name: str
    size: int
    sha256: str


@dataclass(frozen=True)
class ArchiveRecord:
    path: PurePosixPath
    offset: int
    size: int
    md5: str
    sha256: str

    @property
    def end(self) -> int:
        return self.offset + self.size


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
    # A killed process can leave only the temporary file behind.  It was never
    # committed, so replacing it is safe and keeps retries idempotent.
    temporary.unlink(missing_ok=True)
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


def validate_manifest_layout(
    manifest: dict[str, object],
) -> tuple[list[ArchiveRecord], list[PartRecord], int]:
    """Validate the complete byte layout before any destination is changed."""
    raw_files = manifest["files"]
    raw_parts = manifest["parts"]
    assert isinstance(raw_files, list)
    assert isinstance(raw_parts, list)

    expected_count = checked_nonnegative_int(manifest.get("fileCount"), "file count")
    expected_total = checked_nonnegative_int(manifest.get("totalSize"), "total size")
    part_size = checked_nonnegative_int(manifest.get("partSize"), "part size")
    if expected_count != len(raw_files):
        raise TransferError(
            f"manifest file count is {expected_count}, but files[] has "
            f"{len(raw_files)} entries"
        )
    if expected_total and not raw_parts:
        raise TransferError("manifest declares data but has no volumes")
    if expected_total and part_size == 0:
        raise TransferError("manifest declares data but has a zero part size")
    if part_size > 100 * 1024 * 1024:
        raise TransferError("manifest part size exceeds the 100 MiB transfer limit")

    archives: list[ArchiveRecord] = []
    seen_paths: set[str] = set()
    expected_offset = 0
    for index, value in enumerate(raw_files):
        if not isinstance(value, dict):
            raise TransferError(f"invalid files[{index}] record")
        logical = checked_archive_path(value.get("path"))
        key = str(logical).casefold()
        if key in seen_paths:
            raise TransferError(f"case-insensitive duplicate archive path: {logical}")
        seen_paths.add(key)
        offset = checked_nonnegative_int(value.get("offset"), f"offset of {logical}")
        size = checked_nonnegative_int(value.get("size"), f"size of {logical}")
        if size == 0:
            raise TransferError(f"empty BIG archive in manifest: {logical}")
        md5 = checked_hex(value.get("md5"), 32, f"MD5 of {logical}")
        sha256 = checked_hex(value.get("sha256"), 64, f"SHA-256 of {logical}")
        if offset != expected_offset:
            raise TransferError(
                f"non-contiguous stream at {logical}: expected offset "
                f"{expected_offset}, got {offset}"
            )
        archives.append(ArchiveRecord(logical, offset, size, md5, sha256))
        expected_offset += size
    if expected_offset != expected_total:
        raise TransferError(
            f"total size mismatch: manifest {expected_total}, file table {expected_offset}"
        )

    parts: list[PartRecord] = []
    part_total = 0
    for index, value in enumerate(raw_parts):
        if not isinstance(value, dict):
            raise TransferError(f"invalid parts[{index}] record")
        name = checked_part_name(value.get("name"))
        expected_name = f"{PART_PREFIX}{index + 1:04d}{PART_SUFFIX}"
        if name != expected_name:
            raise TransferError(
                f"out-of-order volume: expected {expected_name}, found {name}"
            )
        size = checked_nonnegative_int(value.get("size"), f"size of {name}")
        sha256 = checked_hex(value.get("sha256"), 64, f"SHA-256 of {name}")
        if index + 1 < len(raw_parts) and size != part_size:
            raise TransferError(
                f"wrong size for {name}: expected full part size {part_size}, got {size}"
            )
        if size > part_size:
            raise TransferError(
                f"wrong size for {name}: maximum {part_size}, got {size}"
            )
        if expected_total and size == 0:
            raise TransferError(f"empty transfer volume: {name}")
        parts.append(PartRecord(name, size, sha256))
        part_total += size
    if part_total != expected_total:
        raise TransferError(
            f"total size mismatch: manifest {expected_total}, volumes {part_total}"
        )
    return archives, parts, expected_total


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
    archives, validated_parts, expected_total = validate_manifest_layout(manifest)
    raw_parts = manifest["parts"]
    assert isinstance(raw_parts, list)

    output.mkdir(parents=True, exist_ok=True)
    reader = VolumeReader(manifest_path.parent, raw_parts)
    expected_offset = 0

    print(
        f"Restoring {len(archives)} BIG archives, "
        f"{expected_total / (1024 ** 3):.2f} GiB, "
        f"from {len(validated_parts)} volumes"
    )

    for index, record in enumerate(archives, 1):
        logical = record.path
        size = record.size
        expected_md5 = record.md5
        expected_sha256 = record.sha256

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
        print(f"[{index:3d}/{len(archives)}] {logical} ({action})")

    reader.finish()
    if expected_offset != expected_total or reader.total != expected_total:
        raise TransferError(
            f"total size mismatch: manifest {expected_total}, "
            f"file table {expected_offset}, stream {reader.total}"
        )

    if delete_parts:
        for record in validated_parts:
            (manifest_path.parent / record.name).unlink()
        checksum_path = manifest_path.parent / CHECKSUMS_NAME
        checksum_path.unlink(missing_ok=True)
        print("Transfer volumes deleted after successful restoration")

    print(
        "Done. Use these install folders:\n"
        f"  BFME2_INSTALL={output / 'BFME2'}\n"
        f"  ROTWK_INSTALL={output / 'RotWK'}"
    )


def archive_disk_paths(output: Path, record: ArchiveRecord) -> tuple[Path, Path]:
    target = output.joinpath(*record.path.parts)
    temporary = target.with_name(target.name + ".transfer-tmp")
    return target, temporary


def finalize_ingested_archive(
    output: Path, record: ArchiveRecord, force: bool
) -> None:
    target, temporary = archive_disk_paths(output, record)
    if not temporary.is_file() or temporary.stat().st_size != record.size:
        actual = temporary.stat().st_size if temporary.is_file() else "missing"
        raise TransferError(
            f"incomplete temporary archive {temporary}: expected {record.size}, "
            f"found {actual}"
        )
    actual_md5, actual_sha256, actual_size = digest_file(temporary)
    if (
        actual_size != record.size
        or actual_md5 != record.md5
        or actual_sha256 != record.sha256
    ):
        raise TransferError(
            f"archive checksum mismatch for {record.path}:\n"
            f"  MD5 expected {record.md5}, got {actual_md5}\n"
            f"  SHA-256 expected {record.sha256}, got {actual_sha256}"
        )
    ensure_real_big(temporary)
    if target.exists():
        if existing_matches(target, record.size, record.md5, record.sha256):
            temporary.unlink()
            return
        if not force:
            raise TransferError(
                f"destination exists but does not match the transfer: {target}\n"
                "Use a clean output folder, or pass --force after checking the path."
            )
        target.unlink()
    os.replace(temporary, target)


def load_ingest_state(
    state_path: Path,
    manifest_sha256: str,
    parts: list[PartRecord],
) -> dict[str, object]:
    if not state_path.exists():
        return {
            "schema": INGEST_STATE_SCHEMA,
            "schemaVersion": INGEST_STATE_VERSION,
            "manifestSha256": manifest_sha256,
            "nextPart": 0,
            "streamOffset": 0,
            "complete": False,
        }
    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise TransferError(f"cannot read ingest checkpoint {state_path}: {error}") from error
    if not isinstance(state, dict):
        raise TransferError(f"ingest checkpoint is not an object: {state_path}")
    if (
        state.get("schema") != INGEST_STATE_SCHEMA
        or state.get("schemaVersion") != INGEST_STATE_VERSION
    ):
        raise TransferError(f"unsupported ingest checkpoint: {state_path}")
    if state.get("manifestSha256") != manifest_sha256:
        raise TransferError(
            "ingest checkpoint belongs to a different manifest; use a clean output folder"
        )
    next_part = checked_nonnegative_int(state.get("nextPart"), "checkpoint nextPart")
    stream_offset = checked_nonnegative_int(
        state.get("streamOffset"), "checkpoint streamOffset"
    )
    if next_part > len(parts):
        raise TransferError(
            f"checkpoint nextPart {next_part} exceeds volume count {len(parts)}"
        )
    expected_offset = sum(part.size for part in parts[:next_part])
    if stream_offset != expected_offset:
        raise TransferError(
            f"checkpoint offset mismatch: expected {expected_offset}, got {stream_offset}"
        )
    if not isinstance(state.get("complete"), bool):
        raise TransferError("invalid checkpoint complete flag")
    if state["complete"] and next_part != len(parts):
        raise TransferError("checkpoint is complete before all volumes were consumed")
    return state


def normalize_ingest_output(
    output: Path,
    archives: list[ArchiveRecord],
    committed_offset: int,
    force: bool,
) -> None:
    """Roll temporary files back to the last atomically committed volume."""
    for record in archives:
        target, temporary = archive_disk_paths(output, record)
        covered = min(record.size, max(0, committed_offset - record.offset))
        if covered == record.size:
            if target.is_file() and target.stat().st_size == record.size:
                temporary.unlink(missing_ok=True)
                continue
            if temporary.is_file() and temporary.stat().st_size >= record.size:
                with temporary.open("r+b") as stream:
                    stream.truncate(record.size)
                finalize_ingested_archive(output, record, force)
                continue
            raise TransferError(
                f"checkpoint says {record.path} is complete, but its verified output "
                "is missing; restart into a clean output folder"
            )

        if covered > 0:
            if target.exists():
                if existing_matches(target, record.size, record.md5, record.sha256):
                    temporary.unlink(missing_ok=True)
                    continue
                if not force:
                    raise TransferError(
                        f"destination exists but does not match the transfer: {target}"
                    )
                target.unlink()
            if not temporary.is_file() or temporary.stat().st_size < covered:
                actual = temporary.stat().st_size if temporary.is_file() else "missing"
                raise TransferError(
                    f"checkpoint needs {covered} bytes in {temporary}, found {actual}; "
                    "restart into a clean output folder"
                )
            if temporary.stat().st_size != covered:
                with temporary.open("r+b") as stream:
                    stream.truncate(covered)
            continue

        # Bytes beyond the checkpoint came from an interrupted, uncommitted
        # volume.  They are replayed from that volume on the next invocation.
        temporary.unlink(missing_ok=True)


def consume_verified_part(
    part_path: Path,
    part: PartRecord,
    start_offset: int,
    archives: list[ArchiveRecord],
    output: Path,
    force: bool,
) -> int:
    actual_md5, actual_sha256, actual_size = digest_file(part_path)
    del actual_md5  # Volumes are intentionally authenticated with SHA-256 only.
    if actual_size != part.size:
        raise TransferError(
            f"wrong size for {part.name}: expected {part.size}, found {actual_size}"
        )
    if actual_sha256 != part.sha256:
        raise TransferError(
            f"SHA-256 mismatch for {part.name}: expected {part.sha256}, "
            f"got {actual_sha256}"
        )

    archive_index = 0
    while (
        archive_index < len(archives)
        and archives[archive_index].end <= start_offset
    ):
        archive_index += 1
    position = start_offset
    part_end = start_offset + part.size
    validated_existing: set[PurePosixPath] = set()

    with part_path.open("rb") as source:
        while position < part_end:
            if archive_index >= len(archives):
                raise TransferError(
                    f"volume {part.name} extends beyond the archive table"
                )
            record = archives[archive_index]
            if not (record.offset <= position < record.end):
                raise TransferError(
                    f"archive table has a gap at stream offset {position}"
                )
            local_offset = position - record.offset
            segment_size = min(record.end, part_end) - position
            target, temporary = archive_disk_paths(output, record)
            target.parent.mkdir(parents=True, exist_ok=True)

            skip_write = False
            if target.exists():
                if record.path not in validated_existing:
                    if not existing_matches(target, record.size, record.md5, record.sha256):
                        if not force:
                            raise TransferError(
                                f"destination exists but does not match the transfer: {target}"
                            )
                        target.unlink()
                    else:
                        validated_existing.add(record.path)
                skip_write = target.exists()

            if not skip_write:
                actual_temporary_size = (
                    temporary.stat().st_size if temporary.exists() else 0
                )
                if actual_temporary_size != local_offset:
                    raise TransferError(
                        f"temporary offset mismatch for {record.path}: expected "
                        f"{local_offset}, found {actual_temporary_size}"
                    )
                destination = temporary.open("ab")
            else:
                destination = None

            remaining = segment_size
            try:
                while remaining:
                    block = source.read(min(remaining, IO_CHUNK))
                    if not block:
                        raise TransferError(f"volume ended early: {part.name}")
                    if destination is not None:
                        destination.write(block)
                    remaining -= len(block)
                    position += len(block)
                if destination is not None:
                    destination.flush()
                    os.fsync(destination.fileno())
                    destination.close()
                    destination = None
            except BaseException:
                if destination is not None:
                    destination.close()
                raise

            if position == record.end:
                if not skip_write:
                    finalize_ingested_archive(output, record, force)
                    print(f"    verified archive: {record.path}")
                archive_index += 1

        if source.read(1):
            raise TransferError(f"volume has undeclared trailing bytes: {part.name}")
    return position


def ingest(
    manifest_path: Path,
    output: Path,
    force: bool,
    delete_parts: bool,
) -> None:
    """Consume available volumes sequentially with an atomic per-volume checkpoint."""
    manifest = load_manifest(manifest_path)
    archives, parts, expected_total = validate_manifest_layout(manifest)
    manifest_sha256 = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    output.mkdir(parents=True, exist_ok=True)
    state_path = output / INGEST_STATE_NAME
    state = load_ingest_state(state_path, manifest_sha256, parts)
    next_part = int(state["nextPart"])
    committed_offset = int(state["streamOffset"])

    if state["complete"]:
        print(
            f"Already complete: {len(archives)} archives, "
            f"{expected_total / (1024 ** 3):.2f} GiB"
        )
        return

    normalize_ingest_output(output, archives, committed_offset, force)
    if delete_parts:
        for completed in parts[:next_part]:
            (manifest_path.parent / completed.name).unlink(missing_ok=True)

    consumed = 0
    while next_part < len(parts):
        part = parts[next_part]
        part_path = manifest_path.parent / part.name
        if not part_path.is_file():
            break
        print(
            f"Ingesting [{next_part + 1:02d}/{len(parts)}] {part.name} "
            f"({part.size / (1024 ** 2):.1f} MiB)"
        )
        committed_offset = consume_verified_part(
            part_path, part, committed_offset, archives, output, force
        )
        next_part += 1
        state["nextPart"] = next_part
        state["streamOffset"] = committed_offset
        state["complete"] = next_part == len(parts)
        write_json_atomic(state_path, state)
        if delete_parts:
            part_path.unlink()
        consumed += 1

    if next_part == len(parts):
        if committed_offset != expected_total:
            raise TransferError(
                f"completed offset mismatch: expected {expected_total}, "
                f"got {committed_offset}"
            )
        for record in archives:
            target, _ = archive_disk_paths(output, record)
            if not target.is_file() or target.stat().st_size != record.size:
                raise TransferError(f"completed archive is missing: {target}")
        print(
            f"Done: {len(archives)} verified BIG archives ({expected_total / (1024 ** 3):.2f} GiB).\n"
            f"  BFME2_INSTALL={output / 'BFME2'}\n"
            f"  ROTWK_INSTALL={output / 'RotWK'}"
        )
        return

    waiting = parts[next_part].name
    print(
        f"Checkpoint: {next_part}/{len(parts)} volumes, "
        f"{committed_offset / (1024 ** 3):.2f} GiB committed.\n"
        f"Waiting for {waiting} in {manifest_path.parent}."
    )
    if consumed == 0:
        print("No data was changed.")


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

    ingest_parser = sub.add_parser(
        "ingest",
        help="resume restoration from whichever consecutive volumes are available",
    )
    ingest_parser.add_argument("--manifest", type=Path, required=True)
    ingest_parser.add_argument("--out", type=Path, required=True, help="destination root")
    ingest_parser.add_argument(
        "--force", action="store_true", help="replace mismatching destination files"
    )
    ingest_parser.add_argument(
        "--delete-parts",
        action="store_true",
        help="delete each local volume immediately after its checkpoint is committed",
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
        elif args.command == "ingest":
            ingest(args.manifest, args.out, args.force, args.delete_parts)
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
