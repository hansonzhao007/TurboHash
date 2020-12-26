#include <immintrin.h>
#include <cstdlib>
#include "util/env.h"
#include "util/pmm_util.h"

#include "turbo/turbo_hash.h"

using namespace util;
int main() {
    srand(time(NULL));
    turbo::util::BitSet bitset(0x05);
    for (int i : bitset) {
        printf("has bit in pos: %d\n", i);
    }

    

    {   
        printf("======== Cell Meta 256 V2 Test =========\n");
        uint16_t bitset_buffer256v2[16] =  {
            0xbaaf, 0x0000, // here is the bitmap space
//        LSB 
// bitmap: 0  0   1     1    0      1     0     1  |  0      1    0     1     1     1     0     1
                0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
// index:  0  1   2     3    4      5     6     7     8     9     10    11    12    13    14    15        
        turbo::unordered_map<int, int>::CellMeta256V2 meta256v2((char*)bitset_buffer256v2);
        for (int i : meta256v2.EmptyBitSet()) {
            printf("empty slot %d\n", i);
        }
        for (int i : meta256v2.ValidBitSet()) {
            printf("occupied slot %d\n", i);
        }
        for (int i : meta256v2.MatchBitSet(0x55)) {
            // this should not be print out
            printf("ERROR: 0x55 is in slot: %d\n", i);
        }
        for (int i : meta256v2.MatchBitSet(0x44)) {
            printf("0x44 is in slot: %d\n", i);
        }
    }
    return 0;
}