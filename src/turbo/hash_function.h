#pragma once
namespace turbo {

class MurMurHash {
public:
    static inline size_t hash ( const void * key, int len)
    {
        // return (XXH64(key, len, len));
        return (MurmurHash64A(key, len));
    }

    static inline uint32_t rotl32 ( uint32_t x, int8_t r )
    {
        return (x << r) | (x >> (32 - r));
    }

    static inline uint64_t rotl64 ( uint64_t x, int8_t r )
    {
        return (x << r) | (x >> (64 - r));
    }

    static inline int hash64to32shift(uint64_t key)
    {
        key = (~key) + (key << 18); // key = (key << 18) - key - 1;
        key = key ^ (key >> 31);
        key = key * 21; // key = (key + (key << 2)) + (key << 4);
        key = key ^ (key >> 11);
        key = key + (key << 6);
        key = key ^ (key >> 22);
        return (int) key;
    }

    static uint64_t xorshift(const uint64_t& n,int i){
        return n^(n>>i);
    }

    static uint64_t numhash(const uint64_t& n) {
        uint64_t p = 0x5555555555555555ull; // pattern of alternating 0 and 1
        uint64_t c = 17316035218449499591ull;// random uneven integer constant; 
        return c*xorshift(p*xorshift(n,32),32);
    }
    
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
    static inline uint64_t MurmurHash64A ( const void * key, int len)
    {
        const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
        const uint64_t seed = UINT64_C(0xe17a1465);
        const int r = 47;

        uint64_t h = seed ^ (len * m);

        const uint64_t * data = (const uint64_t *)key;
        const uint64_t * end = data + (len/8);

        while(data != end)
        {
            uint64_t k = *data++;

            k *= m;
            k ^= k >> r;
            k *= m;

            h ^= k;
            h *= m;
        }

        const unsigned char * data2 = (const unsigned char*)data;

        switch(len & 7)
        {
        case 7: h ^= ((uint64_t)data2[6]) << 48;
        case 6: h ^= ((uint64_t)data2[5]) << 40;
        case 5: h ^= ((uint64_t)data2[4]) << 32;
        case 4: h ^= ((uint64_t)data2[3]) << 24;
        case 3: h ^= ((uint64_t)data2[2]) << 16;
        case 2: h ^= ((uint64_t)data2[1]) << 8; 
        case 1: h ^= ((uint64_t)data2[0]);
            h *= m;
        };

        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
    }
#pragma GCC diagnostic pop
};




inline uint64_t lehmer64() {
    thread_local __uint128_t g_lehmer64_state = random();
    g_lehmer64_state *= 0xda942042e4dd58b5;
    return g_lehmer64_state >> 64;
}

inline uint32_t wyhash32() {
    thread_local uint32_t wyhash32_x = random();
    wyhash32_x += 0x60bee2bee120fc15;
    uint64_t tmp;
    tmp = (uint64_t) wyhash32_x * 0xa3b195354a39b70d;
    uint32_t m1 = (tmp >> 32) ^ tmp;
    tmp = (uint64_t)m1 * 0x1b03738712fad5c9;
    uint32_t m2 = (tmp >> 32) ^ tmp;
    return m2;
}

inline uint64_t wyhash64() {
    thread_local uint64_t wyhash64_x = random();
    wyhash64_x += 0x60bee2bee120fc15;
    __uint128_t tmp;
    tmp = (__uint128_t) wyhash64_x * 0xa3b195354a39b70d;
    uint64_t m1 = (tmp >> 64) ^ tmp;
    tmp = (__uint128_t)m1 * 0x1b03738712fad5c9;
    uint64_t m2 = (tmp >> 64) ^ tmp;
    return m2;
}

// https://graphics.stanford.edu/~seander/bithacks.html#SelectPosFromMSBRank
inline uint32_t SelectRandomSetBit(uint64_t v)
{
    uint64_t a = v - ((v >> 1) & ~0UL / 3);
    uint64_t b = (a & ~0UL / 5) + ((a >> 2) & ~0UL / 5);
    uint64_t c = (b + (b >> 4)) & ~0UL / 0x11;
    uint64_t d = (c + (c >> 8)) & ~0UL / 0x101;
    uint64_t t = ((d >> 32) + (d >> 48));

    uint64_t r = (wyhash64() % __builtin_popcount(v)) + 1;
    uint64_t s = 64;

    s -= ((t - r) & 256) >> 3; r -= (t & ((t - r) >> 8));
    t = (d >> (int)(s - 16)) & 0xff;
    s -= ((t - r) & 256) >> 4; r -= (t & ((t - r) >> 8));
    t = (c >> (int)(s - 8)) & 0xf;
    s -= ((t - r) & 256) >> 5; r -= (t & ((t - r) >> 8));
    t = (b >> (int)(s - 4)) & 0x7;
    s -= ((t - r) & 256) >> 6; r -= (t & ((t - r) >> 8));
    t = (a >> (int)(s - 2)) & 0x3;
    s -= ((t - r) & 256) >> 7; r -= (t & ((t - r) >> 8));
    t = (v >> (int)(s - 1)) & 0x1;
    s -= ((t - r) & 256) >> 8;

    return s - 1;
}

}