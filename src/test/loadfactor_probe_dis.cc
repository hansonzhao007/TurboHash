#include "turbo/turbo_hash.h"
#include "util/env.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


int main() {
    turbo::unordered_map<size_t, size_t> map(1, 1024);

    int count = 100 * 1000000;
    for (int i = 0; i < count; i++) {
        map.Put(random(), i);
    }


    // map.PrintAllMeta();

    // printf("%s\n", map.PrintBucketMeta(0).c_str());
    return 0;
}