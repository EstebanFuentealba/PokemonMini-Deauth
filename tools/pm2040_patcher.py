#!/usr/bin/env python3
"""Create and patch single-ROM PM2040 UF2 images.

The layout follows zwenergy/PM2040's single-ROM patcher: a ROMSTART marker
identifies the beginning of a 1 MiB ROM storage area in UF2 address space.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

ROM_MARKER = b"ROMSTART"
MAX_ROM_SIZE = 1024 * 1024
UF2_BLOCK_SIZE = 512
UF2_DATA_OFFSET = 32
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30


def make_header(output: Path) -> None:
    """Emit a compact C initializer for PM2040's reserved ROM area."""
    output.write_text(
        "#include <stdint.h>\n"
        f"const uint8_t rom[{MAX_ROM_SIZE}] "
        '__attribute__((section(".romStorage"))) = "ROMSTART";\n',
        encoding="ascii",
    )


def uf2_payload_map(image: bytearray) -> dict[int, int]:
    if len(image) == 0 or len(image) % UF2_BLOCK_SIZE:
        raise ValueError("UF2 size is not a multiple of 512 bytes")

    address_to_offset: dict[int, int] = {}
    for block_offset in range(0, len(image), UF2_BLOCK_SIZE):
        magic0, magic1 = struct.unpack_from("<II", image, block_offset)
        magic_end = struct.unpack_from("<I", image, block_offset + 508)[0]
        if (magic0, magic1, magic_end) != (
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_MAGIC_END,
        ):
            raise ValueError(f"Invalid UF2 block at offset {block_offset}")
        target_address, payload_size = struct.unpack_from(
            "<II", image, block_offset + 12
        )
        if payload_size > 476:
            raise ValueError(f"Invalid UF2 payload size {payload_size}")
        for index in range(payload_size):
            address_to_offset[target_address + index] = (
                block_offset + UF2_DATA_OFFSET + index
            )
    return address_to_offset


def find_marker(image: bytearray) -> int:
    for block_offset in range(0, len(image), UF2_BLOCK_SIZE):
        target_address, payload_size = struct.unpack_from(
            "<II", image, block_offset + 12
        )
        start = block_offset + UF2_DATA_OFFSET
        marker_offset = image.find(ROM_MARKER, start, start + payload_size)
        if marker_offset >= 0:
            return target_address + marker_offset - start
    raise ValueError("ROMSTART marker not found in base UF2")


def patch_uf2(base: Path, rom_path: Path, output: Path) -> None:
    rom = rom_path.read_bytes()
    if not rom:
        raise ValueError("ROM is empty")
    if len(rom) > MAX_ROM_SIZE:
        raise ValueError(f"ROM exceeds the PM2040 1 MiB limit: {len(rom)} bytes")

    image = bytearray(base.read_bytes())
    payload = uf2_payload_map(image)
    rom_address = find_marker(image)
    for index, value in enumerate(rom):
        file_offset = payload.get(rom_address + index)
        if file_offset is None:
            raise ValueError("Base UF2 does not contain the complete ROM storage area")
        image[file_offset] = value
    output.write_bytes(image)
    print(f"Patched {len(rom)} bytes at RP2040 address 0x{rom_address:08x}")


def main() -> None:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    header = commands.add_parser("make-header")
    header.add_argument("output", type=Path)
    patch = commands.add_parser("patch")
    patch.add_argument("base", type=Path)
    patch.add_argument("rom", type=Path)
    patch.add_argument("output", type=Path)
    args = parser.parse_args()

    if args.command == "make-header":
        make_header(args.output)
    else:
        patch_uf2(args.base, args.rom, args.output)


if __name__ == "__main__":
    main()
