; loader.s — Multiboot-compliant bootloader stub for IncedenaryOS

global loader
global stack_top

; Multiboot header constants
MAGIC_NUMBER equ 0x1BADB002
FLAGS        equ 0x0
CHECKSUM     equ -MAGIC_NUMBER

KERNEL_STACK_SIZE equ 16384

section .text
align 4
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM

loader:
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    push ebx
    push eax
    extern kmain
    call kmain
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb KERNEL_STACK_SIZE
stack_top: