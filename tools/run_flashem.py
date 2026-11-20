#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Launch a direct-load ARM ELF with the standalone FlashEm emulator."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(
        description="Lancer un ELF V.Flash avec FlashEm et une ROM de démarrage minimale."
    )
    parser.add_argument("flashem", type=Path, help="exécutable FlashEm autonome")
    parser.add_argument("elf", type=Path, help="fichier ELF du portage")
    args, emulator_options = parser.parse_known_args()

    executable = args.flashem.expanduser().resolve()
    elf = args.elf.expanduser().resolve()
    for path in (executable, elf):
        if not path.is_file():
            parser.error(f"fichier introuvable : {path}")

    with tempfile.TemporaryDirectory(prefix="doom-vflash-") as directory:
        boot_rom = Path(directory) / "direct-load.bin"
        with boot_rom.open("wb") as rom:
            # ARM's B . instruction at each vector. Direct load enters the ELF.
            rom.write(bytes.fromhex("fe ff ff ea") * 8)
            rom.truncate(2 * 1024 * 1024)
        return subprocess.call(
            [str(executable), *emulator_options, "--boot-rom", str(boot_rom), "--load", str(elf)]
        )


if __name__ == "__main__":
    raise SystemExit(main())
