# Sarp OS v0.2

Minimal 32-bit x86 bare-metal learning kernel written from scratch in C and NASM Assembly, booting via Multiboot in QEMU.

## Features
- **32-bit Protected Mode** entry via Multiboot (`boot.asm`)
- **VGA Text Mode Driver** (`80x25` framebuffer mapped at `0xB8000` with scrolling and hardware cursor)
- **PS/2 Keyboard Polling** (`0x60` / `0x64` ports, Scancode Set 1 with Shift support)
- **Interactive Shell** with built-in commands:
  - `help` ? list available commands
  - `sysinfo` ? display kernel and hardware interface info
  - `palette` ? render 16-color VGA attribute test
  - `matrix` ? real-time VGA framebuffer rain demo
  - `clear` ? clear console
  - `reboot` ? trigger PS/2 controller CPU reset (`0xFE`)

## Project Structure
- `boot.asm` ? Multiboot header, stack setup, and jump to `kernel_main`
- `kernel.c` ? freestanding C kernel, VGA console, PS/2 keyboard driver, and shell
- `linker.ld` ? ELF32 linker script placing the kernel at `1 MiB`
- `run.cmd` ? Windows build & boot script (NASM + LLVM `clang`/`ld.lld` + QEMU)
- `Makefile` ? Linux/WSL build & ISO creation script

## Build & Run
On Windows (with NASM, LLVM, and QEMU paths configured in `run.cmd`):
```cmd
run.cmd
```

On Linux / WSL (with `nasm`, `gcc`, `ld`, `grub-mkrescue`, `qemu-system-x86_64`):
```bash
make run
```
