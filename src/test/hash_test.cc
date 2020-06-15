#include <immintrin.h>
#include <cstdlib>
#include "lightning/hash_table.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;



int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    auto* hashtable = new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinCell>(16, 8, false);
    printf("------- Iterate empty hash table ------\n");
    hashtable->IterateAll();

    for (int i = 0 ; i < 1000; i++) {
        hashtable->Put("key" + std::to_string(i), "value");
    }
    printf("------- Iterate hash table with %lu entries ------\n", hashtable->Size());
    hashtable->IterateAll();

    return 0;
}