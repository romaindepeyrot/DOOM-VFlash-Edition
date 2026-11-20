# DOOM on VTech V.Flash

A port of LinuxDoom 1.10 to the V.Flash's ARM926EJ-S processor. The engine runs inside the console as emulated by FlashEm. This repository contains the port's sources, a prebuilt ARM engine and two Python helper scripts.

> **Status:** the only validated target is loading the ELF in standalone FlashEm. Booting from a disc or on a physical console is untested.

## Quick start

### Requirements

- Python 3
- The standalone **FlashEm** executable with `--load` support
- A Doom **IWAD** data file. The port was tested with `doom1.wad` from DOOM shareware 1.9. The IWAD is **not** included, so you need to provide your own.

### Run

From the extracted folder, run these two commands, adjusting the paths to your setup:

```sh
python3 tools/pack_wad.py doom-vflash-engine.elf doom1.wad doom-vflash.elf
python3 tools/run_flashem.py /path/to/flashem doom-vflash.elf
```

On Windows, point to `flashem.exe` and use `py -3` instead of `python3` if needed. Wrap any path containing spaces in quotes.

What the scripts do:

1. `pack_wad.py` builds a single ELF containing the engine and the IWAD, within the console's 16 MiB of RAM.
2. `run_flashem.py` launches that ELF and gives FlashEm a minimal temporary boot file.

Game logs are printed to your terminal through the emulated UART 1.

## Controls

| Action | V.Flash controller | Default FlashEm keyboard |
| --- | --- | --- |
| Move forward/backward and turn | D-pad | Arrow keys |
| Fire | Red | Z |
| Open a door / confirm | Green | C |
| Change weapon | Yellow | X |
| Automap / back in menus | Blue | V |
| Open menu / confirm | Enter | Enter |
| Cancel / return to game | Exit | Backspace |
| Strafe | Left and right triggers | Q and W |
| Help and pause | Question and Book | H and B |

## Features and limitations

- Save and load from the in-game menu. Saves are kept in RAM for the current session only.
- Stereo PCM sound effects.
- No music.
- No multiplayer.
- Tested in standalone FlashEm only. Disc boot and real hardware are still to be verified.

## Building the engine

### Requirements

- GNU Make
- An `arm-none-eabi` GNU toolchain with newlib
- Python 3

On Debian, install `gcc-arm-none-eabi`, `binutils-arm-none-eabi` and `libnewlib-arm-none-eabi`.

### Build

```sh
make -j
make game WAD="/path/to/doom1.wad"
```

The engine is written to `build/doom-vflash-engine.elf`, and the second command generates `build/doom-vflash.elf`. Set `CROSS_COMPILE` to use a different toolchain prefix.

## Credits and license

The engine is derived from id Software's GPL release of LinuxDoom 1.10, at commit `a77dfb96cb91780ca334d0d4cfd86957558007e0`. See [COPYING](COPYING) and the [id Software repository](https://github.com/id-Software/DOOM).

The port was tested against FlashEm at commit `cb771f893f6a7e0a9c79adbe1081a0d49be3f44b`. The [FlashEm repository](https://github.com/WizzardSK/flashem-libretro) contains the emulator and its build instructions.

DOOM is a trademark of id Software. This project does not distribute any game data, so you must supply your own IWAD.
