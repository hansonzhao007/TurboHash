#pragma once
#include <stdint.h>
namespace turbo {

inline uint64_t lehmer64 () {
    static thread_local __uint128_t g_lehmer64_state = random ();
    g_lehmer64_state *= 0xda942042e4dd58b5;
    return g_lehmer64_state >> 64;
}

inline uint32_t wyhash32 () {
    static thread_local uint32_t wyhash32_x = random ();
    wyhash32_x += 0x60bee2bee120fc15;
    uint64_t tmp;
    tmp = (uint64_t)wyhash32_x * 0xa3b195354a39b70d;
    uint32_t m1 = (tmp >> 32) ^ tmp;
    tmp = (uint64_t)m1 * 0x1b03738712fad5c9;
    uint32_t m2 = (tmp >> 32) ^ tmp;
    return m2;
}

inline uint64_t wyhash64 () {
    static thread_local uint64_t wyhash64_x = random ();
    wyhash64_x += 0x60bee2bee120fc15;
    __uint128_t tmp;
    tmp = (__uint128_t)wyhash64_x * 0xa3b195354a39b70d;
    uint64_t m1 = (tmp >> 64) ^ tmp;
    tmp = (__uint128_t)m1 * 0x1b03738712fad5c9;
    uint64_t m2 = (tmp >> 64) ^ tmp;
    return m2;
}

// https://graphics.stanford.edu/~seander/bithacks.html#SelectPosFromMSBRank
inline uint32_t SelectRandomSetBit (uint64_t v) {
    uint64_t a = v - ((v >> 1) & ~0UL / 3);
    uint64_t b = (a & ~0UL / 5) + ((a >> 2) & ~0UL / 5);
    uint64_t c = (b + (b >> 4)) & ~0UL / 0x11;
    uint64_t d = (c + (c >> 8)) & ~0UL / 0x101;
    uint64_t t = ((d >> 32) + (d >> 48));

    uint64_t r = (wyhash64 () % __builtin_popcount (v)) + 1;
    uint64_t s = 64;

    s -= ((t - r) & 256) >> 3;
    r -= (t & ((t - r) >> 8));
    t = (d >> (int)(s - 16)) & 0xff;
    s -= ((t - r) & 256) >> 4;
    r -= (t & ((t - r) >> 8));
    t = (c >> (int)(s - 8)) & 0xf;
    s -= ((t - r) & 256) >> 5;
    r -= (t & ((t - r) >> 8));
    t = (b >> (int)(s - 4)) & 0x7;
    s -= ((t - r) & 256) >> 6;
    r -= (t & ((t - r) >> 8));
    t = (a >> (int)(s - 2)) & 0x3;
    s -= ((t - r) & 256) >> 7;
    r -= (t & ((t - r) >> 8));
    t = (v >> (int)(s - 1)) & 0x1;
    s -= ((t - r) & 256) >> 8;

    return s - 1;
}

}  // namespace turbo