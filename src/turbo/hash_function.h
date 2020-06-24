#pragma once

namespace turbo {

class MurMurHash {
public:
    static inline size_t hash ( const void * key, int len)
    {
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
};

}