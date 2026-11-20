#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Combine the ARM engine and a user-provided IWAD into a FlashEm ELF."""
import argparse
from pathlib import Path
import struct

SDRAM_START = 0x10000000
SDRAM_END = 0x11000000
METADATA = SDRAM_END - 16
MIN_HEAP = 2 * 1024 * 1024 + 704 * 1024
PAGE = 4096

ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
WAD_HEADER = struct.Struct("<4sII")
WAD_ENTRY = struct.Struct("<II8s")


def wad_mode(wad):
    if len(wad) < WAD_HEADER.size:
        raise ValueError("en-tête WAD incomplet")
    magic, count, directory = WAD_HEADER.unpack_from(wad)
    if (magic != b"IWAD" or directory < WAD_HEADER.size or not count
            or directory + count * WAD_ENTRY.size > len(wad)):
        raise ValueError("IWAD ou répertoire de ressources invalide")

    names = set()
    for offset, size, name in WAD_ENTRY.iter_unpack(
        memoryview(wad)[directory:directory + count * WAD_ENTRY.size]
    ):
        if size and offset + size > len(wad):
            raise ValueError(f"ressource hors du fichier {name.rstrip(bytes(1))!r}")
        names.add(name.rstrip(bytes(1)).upper())

    if b"MAP01" in names:
        return 2  # commercial
    if b"E1M1" not in names:
        raise ValueError("aucun niveau de départ compatible")
    if b"E4M1" in names:
        return 3  # retail
    if b"E2M1" in names:
        return 1  # registered
    return 0  # shareware


def read_engine(engine):
    if len(engine) < ELF_HEADER.size:
        raise ValueError("en-tête ELF incomplet")
    ident, kind, machine, version, entry, phoff, _, flags, _, phsize, phnum, *_ = (
        ELF_HEADER.unpack_from(engine)
    )
    if (ident[:7] != b"\x7fELF\x01\x01\x01" or kind != 2 or machine != 40
            or version != 1 or phsize != PROGRAM_HEADER.size
            or phoff + phnum * phsize > len(engine)):
        raise ValueError("le moteur doit être un exécutable ELF ARM 32 bits little-endian")

    segments = []
    for index in range(phnum):
        pt, offset, vaddr, paddr, filesz, memsz, perm, _ = PROGRAM_HEADER.unpack_from(
            engine, phoff + index * phsize
        )
        if pt != 1:
            continue
        if (vaddr != paddr or filesz > memsz or offset + filesz > len(engine)
                or paddr < SDRAM_START or paddr + memsz > METADATA
                or paddr % PAGE):
            raise ValueError("le segment du moteur ne correspond pas à la SDRAM V.Flash")
        segments.append((paddr, memsz, perm, engine[offset:offset + filesz]))

    if len(segments) != 1:
        raise ValueError("le moteur doit avoir un seul segment chargeable")
    address, length, perm, _ = segments[0]
    if not (perm & 1 and address <= entry < address + length):
        raise ValueError("point d’entrée du moteur invalide")
    return ident, flags, entry, segments


def pack(engine, wad):
    mode = wad_mode(wad)
    ident, flags, entry, segments = read_engine(engine)
    address = (METADATA - len(wad)) & -PAGE
    engine_end = max(start + length for start, length, _, _ in segments)
    if address - engine_end < MIN_HEAP:
        max_size = METADATA - ((engine_end + MIN_HEAP + PAGE - 1) & -PAGE)
        raise ValueError(f"IWAD trop volumineux pour ce moteur (maximum {max_size:,} octets)")

    padding = METADATA - address - len(wad)
    metadata = struct.pack("<4sIII", b"VWAD", address, len(wad), mode)
    segments.append((address, SDRAM_END - address, 4, wad + bytes(padding) + metadata))

    result = bytearray(ELF_HEADER.pack(
        ident, 2, 40, 1, entry, ELF_HEADER.size, 0, flags, ELF_HEADER.size,
        PROGRAM_HEADER.size, len(segments), 0, 0, 0
    ))
    for start, length, permissions, content in segments:
        file_offset = (ELF_HEADER.size + len(segments) * PROGRAM_HEADER.size + PAGE - 1) & -PAGE
        if start != segments[0][0]:
            file_offset += (len(segments[0][3]) + PAGE - 1) & -PAGE
        result.extend(PROGRAM_HEADER.pack(
            1, file_offset, start, start, len(content), length, permissions, PAGE
        ))

    for _, _, _, content in segments:
        result.extend(bytes((-len(result)) % PAGE))
        result.extend(content)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("wad", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    try:
        if args.output.resolve() in {args.engine.resolve(), args.wad.resolve()}:
            raise ValueError("utilisez un fichier de sortie distinct")
        if args.wad.stat().st_size >= SDRAM_END - SDRAM_START:
            raise ValueError("IWAD trop volumineux pour les 16 Mio de la console")
        elf = pack(args.engine.read_bytes(), args.wad.read_bytes())
        args.output.write_bytes(elf)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Erreur : {error}\n")
    print(args.output)


if __name__ == "__main__":
    main()
