.section .text
.code32
.global syscall

syscall:
    push %ebp
    mov %esp, %ebp

    mov 8(%ebp), %eax
    mov 12(%ebp), %ebx
    mov 16(%ebp), %ecx
    mov 20(%ebp), %edx

    int $0x80

    pop %ebp
    ret
