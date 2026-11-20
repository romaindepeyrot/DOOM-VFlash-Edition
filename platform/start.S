/* SPDX-License-Identifier: GPL-2.0-or-later */
.syntax unified
.arm
.section .text.start, "ax", %progbits
.global _start
.type _start, %function

_start:
    msr cpsr_c, #0xd3
    ldr sp, =__stack_top
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    bl vflash_start
2:
    b 2b

.size _start, . - _start
