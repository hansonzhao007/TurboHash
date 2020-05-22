#include <immintrin.h>
#include <cstdlib>
#include <unordered_set>
#include "util/env.h"

#include "lightning/hash_table.h"
#include "util/robin_hood.h"
#include "util/io_report.h"

#include "absl/container/flat_hash_set.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;

DEFINE_int32(associate_size, 8, "");
DEFINE_int32(bucket_size, 1 << 20, "bucket count");
DEFINE_int32(probe_type, 0, "\
    0: probe within bucket, \
    1: probe two buckets");
DEFINE_int32(cell_type, 0, "\
    0: 128 byte cell, \
    1: 256 byte cell");
DEFINE_bool(locate_cell_with_h1, false, "using partial hash h1 to locate cell inside bucket or not");

lthash::HashTable* HashTableCreate(int cell_type, int probe_type, int bucket, int associate) {
    if (0 == cell_type && 0 == probe_type)
        return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
    if (0 == cell_type && 1 == probe_type)
        return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeContinousTwoBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
    if (1 == cell_type && 0 == probe_type)
        return new lthash::DramHashTable<lthash::CellMeta256, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
    if (1 == cell_type && 1 == probe_type)
        return new lthash::DramHashTable<lthash::CellMeta256, lthash::ProbeContinousTwoBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
    else
        return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
}

template <class HashType>
void HashSpeedTest(const std::string& name, size_t inserted_num);

int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    size_t inserted_num = 0;
    {
        util::Stats stats;
        lthash::HashTable& hashtable = *HashTableCreate(FLAGS_cell_type, FLAGS_probe_type, FLAGS_bucket_size, FLAGS_associate_size);
        std::string key = "ltkey";
        uint64_t i = 0;
        bool res = true;
        auto time_start = Env::Default()->NowNanos();
        while (res) {
            res = hashtable.Put(key + std::to_string(i++), "value123");
            if ((i & 0xFFFFF) == 0) {
                fprintf(stderr, "%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        printf("lthash(%25s) - Load Factor: %.2f. Inserted %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", hashtable.ProbeStrategyName().c_str(), hashtable.LoadFactor(), hashtable.Size(), (double)hashtable.Size() / (time_end - time_start) * 1000.0, (time_end - time_start));
        inserted_num = hashtable.Size();

        i = 0;
        res = true;
        time_start = Env::Default()->NowNanos();
        std::string value;
        while (res) {
            res = hashtable.Get(key + std::to_string(i++), &value);
            if ((i & 0xFFFFF) == 0) {
                fprintf(stderr, "%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        time_end = Env::Default()->NowNanos();
        printf("lthash(%25s) - Load Factor: %.2f. Read     %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", hashtable.ProbeStrategyName().c_str(), hashtable.LoadFactor(), i, (double)i / (time_end - time_start) * 1000.0, (time_end - time_start));
    }
    
    return 0;
}