#include <immintrin.h>
#include <cstdlib>
#include "turbo/hash_table.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


const size_t COUNT = 100000;
int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    auto* hashtable = new turbo::DramHashTable<turbo::CellMeta128, turbo::ProbeWithinBucket>(16, 32);
    printf("------- Iterate empty hash table ------\n");
    hashtable->IterateAll();

    bool succ = true;
    size_t i = 0;
    for (i = 0 ; i < COUNT && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "value" + std::to_string(i));
    }
    printf("inserted %lu kv, hashtable size: %lu, loadfactor: %f\n", i, hashtable->Size(), hashtable->LoadFactor());

    auto read_fun = [&hashtable] {
        bool succ = true;
        std::string value;
        size_t i = 0;
        for (; i < COUNT && succ; i++) {
            succ = hashtable->Get("key" + std::to_string(i), &value);
        }
        printf("find %lu key, hashtable size: %lu, loadfactor: %f\n", i, hashtable->Size(), hashtable->LoadFactor());
    };
    read_fun();
    // printf("------- Iterate hash table with %lu entries ------\n", hashtable->Size());
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    hashtable->IterateValidBucket();
    
    printf("------- rehash all bucket and repeat search 0 ------\n");
    hashtable->ReHashAll();
    hashtable->IterateValidBucket();
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    read_fun();

    hashtable->ReHashAll();
    read_fun();
    return 0;
}