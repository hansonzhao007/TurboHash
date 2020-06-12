#include <immintrin.h>
#include <cstdlib>
#include "lightning/hash_table.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;



int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);

    for (int i = 0; i < 100; i++) {
        volatile auto* hashtable = new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinCell>(512 << 10, 8, false);

        delete hashtable;
    }
    return 0;
}