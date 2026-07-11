#include "errno-limits.h"

.global _start
.text

# x64: rax | rdi   rsi   rdx   r10   r8   x r9
# x86: eax | ebx   ecx   edx   esi   edi  x ebp
# C  : rdi | rsi   rdx   rcx   r8    r9   x [rsp+8]

syscall_ptr:
    .quad 0x0000000000000000
errno_offset:
    .quad 0x0000000000000000

_syscall:
    push %rbp
    mov %rsp, %rbp
    sub $(ERRNO_OFFSET_MAX - ERRNO_OFFSET_MIN + 32), %rsp

# Prepare registers. rcx is syscall number, rdx is stack of arguments
    mov %rcx, %rdi
    mov %rdx, %rbx
    movl 0(%rbx) , %esi
    movl 4(%rbx) , %edx
    movl 8(%rbx) , %ecx
    movl 12(%rbx), %r8d
    movl 16(%rbx), %r9d

# Now rax and rbx are free temporaries we can clobber

# Store original fsbase on stack - we will need to restore it
    rdfsbase %rax
    mov %rax, 8(%rsp)

# Set fs base - we emulate tls on stack
    lea (24 - ERRNO_OFFSET_MIN)(%rsp), %rax
    mov %rax, (%rax)
    wrfsbase %rax

    call *syscall_ptr(%rip)

# If errno has failed...
    cmp $-1, %rax
    jne done

#...load back the errno from stack we prepared
    lea (24 - ERRNO_OFFSET_MIN)(%rsp), %rax
    add errno_offset(%rip), %rax
    movl 0(%rax), %eax
    neg %rax

done:
    mov 8(%rsp), %rbx
    wrfsbase %rbx
    add $(ERRNO_OFFSET_MAX - ERRNO_OFFSET_MIN + 32), %rsp
    mov %rbp, %rsp
    pop %rbp
    ret
