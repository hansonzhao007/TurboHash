#include <immintrin.h>
#include <cstdlib>
#include "util/env.h"
#include "util/pmm_util.h"

#include "turbo/turbo_hash.h"

using namespace util;
int main() {
    char tmp_buf[128];
    sprintf(tmp_buf, "%0.*lu", 15, 123);
    printf("%s\n", tmp_buf);


    srand(time(NULL));
    turbo::util::BitSet bitset(0x05);
    for (int i : bitset) {
        printf("has bit in pos: %d\n", i);
    }
    volatile size_t hash_val;
    auto time_start = util::Env::Default()->NowMicros();
    for (size_t i = 0; i < 100000000; i++) {
        hash_val = turbo::hash<size_t>{}(i);
    }
    auto time_duration = util::Env::Default()->NowMicros();
    printf("duration for 100 million std::hash<size_t>: %f. %lu\n", time_duration / 1000000.0, hash_val);

    std::string value(15, 'a');
    time_start = util::Env::Default()->NowMicros();
    for (size_t i = 0; i < 100000000; i++) {
        hash_val = turbo::hash<std::string>{}(value);
    }
    time_duration = util::Env::Default()->NowMicros();
    printf("duration for 100 million std::hash<std::string>: %f. %lu\n", time_duration / 1000000.0, hash_val);
    

    {   
        printf("======== Cell Meta 256 V2 Test =========\n");
        uint16_t bitset_buffer256v2[16] =  {
            0xbaaf, 0x0000, // here is the bitmap space
//        LSB 
// bitmap: 0  0   1     1    0      1     0     1  |  0      1    0     1     1     1     0     1
                0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
// index:  0  1   2     3    4      5     6     7     8     9     10    11    12    13    14    15        
        turbo::unordered_map<int, double>::CellMeta256V2 meta256v2((char*)bitset_buffer256v2);
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