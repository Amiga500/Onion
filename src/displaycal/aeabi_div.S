.syntax unified
.arm
.text
.align 2
.global __aeabi_uidiv
.type __aeabi_uidiv,%function
__aeabi_uidiv:
    b __aeabi_uidivmod
.size __aeabi_uidiv, .-__aeabi_uidiv

.global __aeabi_uidivmod
.type __aeabi_uidivmod,%function
__aeabi_uidivmod:
    cmp r1, #0
    beq 2f
    mov r2, #0          @ quotient
    mov r3, #0          @ remainder
    mov r12, #32
1:
    lsls r0, r0, #1     @ next numerator bit -> carry
    adc r3, r3, r3      @ rem = rem*2 + carry
    cmp r3, r1
    adc r2, r2, r2      @ q = q*2 + (rem >= divisor)
    subcs r3, r3, r1
    subs r12, r12, #1
    bne 1b
    mov r0, r2
    mov r1, r3
    bx lr
2:
    mov r0, #0
    mov r1, #0
    bx lr
.size __aeabi_uidivmod, .-__aeabi_uidivmod
