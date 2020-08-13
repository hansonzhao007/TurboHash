#include <immintrin.h>
#include <cstdlib>
#include "util/env.h"
#include "util/pmm_util.h"

#include "turbo/hash_table.h"

using namespace util;
int main() {
    srand(time(NULL));
    turbo::BitSet bitset(0x05);
    for (int i : bitset) {
        printf("has bit in pos: %d\n", i);
    }

    {   
        uint8_t bitset_buffer[] = {0x00, 0x00};
        turbo::CellMeta128 meta((char*)bitset_buffer);
        auto bitset_occupy = meta.OccupyBitSet();
        printf("bitset_occupy bitmap: 0x%x\n", bitset_occupy.bit());
        printf("bitset_occupy should be empty: %s\n", bitset_occupy ? "ERROR: not empty" : "empty");

        printf("======== Cell Meta 128 Test =========\n");
        uint8_t bitset_buffer128[16] =  {
                0xac, 0xba, // here is the bitmap space
// bitmap: 0  0   1     1    0      1     0     1  |  0      1    0     1     1     1     0     1
                0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
// index:  0  1   2     3    4      5     6     7     8     9     10    11    12    13    14    15           
        turbo::CellMeta128 meta128((char*)bitset_buffer128);
        for (int i : meta128.EmptyBitSet()) {
            printf("empty slot %d\n", i);
        }
        for (int i : meta128.OccupyBitSet()) {
            printf("occupied slot %d\n", i);
        }
        for (int i : meta128.MatchBitSet(0x55)) {
            // this should not be print out
            printf("ERROR: 0x55 is in slot: %d\n", i);
        }
        for (int i : meta128.MatchBitSet(0x44)) {
            printf("0x44 is in slot: %d\n", i);
        }

        printf("======== Cell Meta 256 Test =========\n");
        uint8_t bitset_buffer256[32] =  {
            0x30, 0x8B, 0xC7, 0xCD, // here is the bitmap space
// bitmap:   1      1    0     0     1     1     0     1     0     0
// index :   4      5    6     7     8     9     10    11    12    13
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 
// bitmap:   0      1    1     1     1     0     0     0     1     1
// index :   14     15   16    17    18    19    20    21    22    23        
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
// bitmap:   1      0    1     1     0     0     1     1
// index :   24     25   26    27    28    29    30    31
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        turbo::CellMeta256 meta256((char*)bitset_buffer256);
        for (int i : meta256.MatchBitSet(0x99)) {
            printf("0x99 is in slot: %d\n", i);
        }

        for (int i : meta256.EmptyBitSet()) {
            printf("empty slot %d\n", i);
        }

        for (int i : meta256.OccupyBitSet()) {
            printf("occupied slot %d\n", i);
        }

        for (int i : meta256.MatchBitSet(0x77)) {
            printf("0x77 is in slot: %d\n", i);
        }

    }
    

    #ifdef __SIZEOF_INT128__
        printf("Support int128\n");
    #else
        printf("Not Support int128\n");
    #endif
    
    turbo::BitSet randomPickBit(0x0000FFFC);
    for (int i = 0; i < 16; i++) {
        printf("Random pick one bit: %d\n", randomPickBit.pickOne());
    }

    auto time_start = util::Env::Default()->NowMicros();
    int pick_time = 100000000;
    for (int i = 0; i < pick_time; ++i) {
        volatile int res = randomPickBit.pickOne();
    }
    auto time_end  = util::Env::Default()->NowMicros();

    printf("Random Pick Speed: %f Mops/s\n", (double)pick_time / (time_end - time_start));
    return 0;
}