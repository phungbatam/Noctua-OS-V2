.section .text
.code32
.global _start

_start:
    xor %ebp, %ebp

    mov (%esp), %ecx
    lea 4(%esp), %edx
    lea 4(%esp,%ecx,4), %ebx
    add $4, %ebx

    push %ebx
    push %edx
    push %ecx
    call main

    push %eax
    call exit

    hlt
