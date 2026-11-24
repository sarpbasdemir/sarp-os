; Multiboot1 header (GRUB / QEMU tarafindan 32-bit protected mode'da yuklenir)
BITS 32

section .multiboot progbits alloc noexec nowrite align=4
    dd 0x1BADB002              ; Multiboot magic number
    dd 0x00                    ; Flags
    dd -(0x1BADB002 + 0x00)    ; Checksum (magic + flags + checksum == 0)

section .text
global _start
extern kernel_main

_start:
    cli                        ; Kesmeleri kapat
    mov esp, stack_top         ; 16 KB stack ayarla
    call kernel_main           ; C cekirdegine atla
.hang:
    hlt                        ; Islemciyi durdur
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384                 ; 16 KB stack
stack_top: