#include <immintrin.h>
#include <cstdlib>
#include "turbo/hash_table.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


void OtherTest() {
    const size_t count = 100000000;
    {
        auto time_start = util::Env::Default()->NowMicros();
        for (size_t i = 0; i < count; ++i) {
            volatile uint64_t rnd_num = turbo::wyhash64();
        }
        auto time_end   = util::Env::Default()->NowMicros();
        printf("wyhash64 speed: %f Mops/s\n", (double)count / (time_end - time_start));
    }
    
    {
        auto time_start = util::Env::Default()->NowMicros();
        for (size_t i = 0; i < count; ++i) {
            volatile uint64_t rnd_num = turbo::lehmer64();
        }
        auto time_end   = util::Env::Default()->NowMicros();
        printf("lehmer64 speed: %f Mops/s\n", (double)count / (time_end - time_start));
    }

    {
        auto time_start = util::Env::Default()->NowMicros();
        for (size_t i = 0; i < count; ++i) {
            volatile uint64_t rnd_num = turbo::wyhash32();
        }
        auto time_end   = util::Env::Default()->NowMicros();
        printf("wyhash32 speed: %f Mops/s\n", (double)count / (time_end - time_start));
    }

}

const size_t COUNT = 100000;
int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    OtherTest();
    auto* hashtable = new turbo::DramHashTable<turbo::CellMeta128, turbo::ProbeWithinBucket>(8, 8192);
    printf("------- Iterate empty hash table ------\n");
    hashtable->IterateAll();

    bool succ = true;
    size_t find = 0;
    for (size_t i = 0 ; i < COUNT && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "value" + std::to_string(i));
        if (likely(succ)) find++;
    }
    printf("inserted %lu kv, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());

    auto read_fun = [&hashtable] {
        bool succ = true;
        std::string value;
        size_t find = 0;
        for (size_t i = 0; i < COUNT && succ; i++) {
            succ = hashtable->Get("key" + std::to_string(i), &value);
            if (likely(succ)) find++;
        }
        printf("find %lu key, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());
    };
    read_fun();
    // printf("------- Iterate hash table with %lu entries ------\n", hashtable->Size());
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    // hashtable->IterateValidBucket();
    
    printf("------- rehash all bucket and repeat search ------\n");
    hashtable->MinorReHashAll();
    // hashtable->IterateValidBucket();
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    read_fun();

    hashtable->MinorReHashAll();
    hashtable->DebugInfo();
    read_fun();
    return 0;
}