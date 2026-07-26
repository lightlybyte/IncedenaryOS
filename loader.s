; loader.s — Multiboot-compliant bootloader stub for IncedenaryOS

global loader
global stack_top                    ; Export stack symbol for debugging

; Multiboot header constants
MAGIC_NUMBER equ 0x1BADB002
FLAGS        equ 0x0
CHECKSUM     equ -MAGIC_NUMBER

KERNEL_STACK_SIZE equ 16384          ; 16 KiB stack (plenty for early kernel)

section .text
align 4
    ; Multiboot header — GRUB requires this to load the kernel
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM

loader:
    ; Set up the stack pointer
    mov esp, kernel_stack + KERNEL_STACK_SIZE

    ; Push the Multiboot info structure address (GRUB passes this in EBX)
    push ebx

    ; Push the magic number (GRUB passes this in EAX)
    push eax

    ; Call the C kernel entry point
    extern kmain
    call kmain

    ; If kmain ever returns, halt the CPU
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb KERNEL_STACK_SIZE
stack_top:                           ; Useful for debugging stack overflows