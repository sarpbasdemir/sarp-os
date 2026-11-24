@echo off
setlocal
set NASM=%LOCALAPPDATA%\bin\NASM\nasm.exe
set CLANG=C:\Users\asus\Tools\llvm\LLVM\bin\clang.exe
set LLD=C:\Users\asus\Tools\llvm\LLVM\bin\ld.lld.exe
set QEMU=C:\Users\asus\Tools\qemu\qemu-system-i386.exe

cd /d "%~dp0"
"%NASM%" -f elf32 boot.asm -o boot.o || exit /b 1
"%CLANG%" --target=i386-none-elf -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -c kernel.c -o kernel.o || exit /b 1
"%LLD%" -m elf_i386 --omagic -T linker.ld -o sarp-os.bin boot.o kernel.o || exit /b 1
"%QEMU%" -kernel sarp-os.bin