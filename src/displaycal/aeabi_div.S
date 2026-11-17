.syntax unified
.arm
.text
.align 2
.global __aeabi_uidiv
.type __aeabi_uidiv,%function
__aeabi_uidiv:
    b __aeabi_uidivmod
.size __aeabi_uidiv, .-__aeabi_uidiv

/* Restoring shift-subtract division, 32 iterations.
 * LIMITATION: the running remainder (r3) is doubled before comparison, so a
 * divisor > 0x80000000 can overflow r3 and give a wrong result. DisplayCal
 * only divides by small values, so this is never reached; replace with
 * libgcc's routine before reusing this file elsewhere.
 * Division by zero returns quotient 0, remainder 0 (no trap).
 */
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

/* Clang freestanding signed-division import; preserves ABI sign semantics. */
.global __aeabi_idiv
.type __aeabi_idiv,%function
__aeabi_idiv:
    push {r4,lr}
    mov r4, #0
    cmp r0,#0
    rsblt r0,r0,#0
    eorlt r4,r4,#1
    cmp r1,#0
    rsblt r1,r1,#0
    eorlt r4,r4,#1
    bl __aeabi_uidiv
    tst r4,#1
    rsbne r0,r0,#0
    pop {r4,pc}
.size __aeabi_idiv, .-__aeabi_idiv
