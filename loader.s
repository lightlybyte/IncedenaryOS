; loader.s — Multiboot-compliant bootloader stub
global loader
global stack_top

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
    ; Set up stack
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    
    ; Save multiboot info for kmain
    ; kmain(unsigned int magic, unsigned int addr)
    ; eax = magic (0x2BADB002)
    ; ebx = multiboot info structure address
    
    ; Push arguments in reverse order (cdecl calling convention)
    push ebx    ; push addr (multiboot info structure)
    push eax    ; push magic number
    
    ; Call kernel main
    extern kmain
    call kmain
    
    ; If kmain ever returns, halt the system
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
kernel_stack:
    resb KERNEL_STACK_SIZE
stack_top: