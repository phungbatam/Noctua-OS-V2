section .multiboot
align 4
    dd 0x1BADB002
    dd 0x07
    dd -(0x1BADB002 + 0x07)
    dd 0
    dd 1024
    dd 768
    dd 32

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    push ebx
    call kmain

    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:
