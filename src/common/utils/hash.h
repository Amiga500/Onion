#ifndef UTILS_HASH_H__
#define UTILS_HASH_H__
// Dedicated to Pippip, the main character in the 'Das Totenschiff' roman,
// actually the B.Traven himself, his real name was Hermann Albert Otto
// Maksymilian Feige. Many thanks go to Yurii 'Hordi' Hordiienko, he lessened
// with 3 instructions the original 'Pippip', thus:
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Zero-extending little-endian load of the first `len` (<= 8) bytes. memcpy
// keeps the produced hash values bit-identical to a plain 8-byte dereference
// while never reading past the buffer and never assuming alignment: ARMv7 LDRD
// traps on unaligned addresses, and compilers fold this back into a single
// load. The original 'Pippip' relied on the caller over-allocating 8 bytes and
// masked the surplus away with (x << n) >> n; zeroing them here is equivalent.
static inline uint64_t _hash_load64le(const char *str, size_t len)
{
    uint64_t value = 0;
    memcpy(&value, str, len);
    return value;
}

uint32_t FNV1A_Pippip_Yurii(const char *str, size_t wrdlen)
{
    const uint32_t PRIME = 591798841;
    uint32_t hash32;
    uint64_t hash64 = 14695981039346656037ULL;
    size_t Cycles, NDhead;
    if (wrdlen > 8) {
        Cycles = ((wrdlen - 1) >> 4) + 1;
        NDhead = wrdlen - (Cycles << 3);
        // #pragma nounroll
        for (; Cycles--; str += 8) {
            hash64 = (hash64 ^ _hash_load64le(str, 8)) * PRIME;
            hash64 = (hash64 ^ _hash_load64le(str + NDhead, 8)) * PRIME;
        }
    }
    else
        hash64 = (hash64 ^ _hash_load64le(str, wrdlen)) * PRIME;
    hash32 = (uint32_t)(hash64 ^ (hash64 >> 32));
    return hash32 ^ (hash32 >> 16);
} // Last update: 2019-Oct-30, 14 C lines strong, Kaze.

// https://godbolt.org/z/i40ipj x86-64 gcc 9.2 -O3
/*
FNV1A_Pippip_Yurii:                              FNV1A_Pippip(char const*,
unsigned int): mov     rax, QWORD PTR [rdi]                    mov     rax,
QWORD PTR [rdi] cmp     rsi, 8                                  cmp     esi, 8
        jbe     .L2                                     jbe     .L2
        lea     rax, [rsi-1]                            lea     ecx, [rsi-1]
        shr     rax, 4                                  xor     edx, edx
        lea     rdx, [8+rax*8]                          shr     ecx, 4
        movabs  rax, -3750763034362895579               add     ecx, 1
        sub     rsi, rdx                                lea     eax, [0+rcx*8]
        add     rdx, rdi                                sub     esi, eax
                                                        movabs  rax,
-3750763034362895579 movsx   rsi, esi add     rsi, rdi .L3: .L4: xor     rax,
QWORD PTR [rdi]                    xor     rax, QWORD PTR [rdi+rdx*8] add rdi, 8
imul    rax, rax, 591798841 imul    rax, rax, 591798841                     xor
rax, QWORD PTR [rsi+rdx*8] xor     rax, QWORD PTR [rdi-8+rsi]              add
rdx, 1 imul    rax, rax, 591798841                     imul    rax, rax,
591798841 cmp     rdi, rdx                                cmp     ecx, edx jne
.L3                                     jg      .L4 .L4: .L3: mov     rdx, rax
mov     rdx, rax shr     rdx, 32                                 shr     rdx, 32
        xor     eax, edx                                xor     eax, edx
        mov     edx, eax                                mov     edx, eax
        shr     edx, 16                                 shr     edx, 16
        xor     eax, edx                                xor     eax, edx
        ret                                             ret
.L2:                                            .L2:
        movabs  rdx, -3750763034362895579               movabs  rdx,
-3750763034362895579 mov     ecx, 8                                  mov ecx, 8
        sub     ecx, esi                                sub     ecx, esi
        sal     ecx, 3                                  sal     ecx, 3
        sal     rax, cl                                 sal     rax, cl
        shr     rax, cl                                 shr     rax, cl
        xor     rax, rdx                                xor     rax, rdx
        imul    rax, rax, 591798841                     imul    rax, rax,
591798841 jmp     .L4                                     jmp     .L3
*/
#endif // UTILS_HASH_H__
