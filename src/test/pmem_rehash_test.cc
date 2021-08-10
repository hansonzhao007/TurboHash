#include "turbo/turbo_hash_pmem.h"
// #include "turbo/turbo_hash.h"
#include "gflags/gflags.h"
#include "util/logger.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

#ifdef TURBO_HASH_H_
using HashTable = turbo::unordered_map<std::string, std::string>;
#define hashnamespace turbo
#elif defined TURBO_HASH_PMEM_H_
using HashTable = turbo_pmem::unordered_map<std::string, std::string>;
#define hashnamespace turbo_pmem
#endif

int main () {
    remove ("/mnt/pmem/turbo_hash_pmem_basemd");
    remove ("/mnt/pmem/turbo_hash_pmem_desc");
    remove ("/mnt/pmem/turbo_hash_pmem_sb");
    const size_t COUNT = 1000000;

    auto* hashtable = new HashTable (4096, 16);

    bool succ = true;
    size_t find = 0;

    auto thread_info = hashtable->getThreadInfo ();

    for (size_t i = 0; i < COUNT && succ; i++) {
        succ =
            hashtable->Put ("key" + std::to_string (i), "value" + std::to_string (i), thread_info);
        if ((succ)) find++;
    }
    printf ("inserted %lu kv, hashtable size: %lu, capacity: %lu, loadfactor: %f\n", find,
            hashtable->Size (), hashtable->Capacity (), hashtable->LoadFactor ());

    auto read_fun = [&hashtable] {
        size_t find = 0;
        std::string value_buffer;
        auto read_callback = [&] (HashTable::RecordType record) { value_buffer = record.value (); };
        auto tinfo = hashtable->getThreadInfo ();
        for (size_t i = 0; i < COUNT; i++) {
            std::string key = "key" + std::to_string (i);
            auto res = hashtable->Find (key, tinfo, read_callback);
            if (i < 10 || (i & 0x7FFF) == 0) {
                INFO ("Get key: %s. val: %s\n", key.c_str (), value_buffer.c_str ());
            }
            if (res) find++;
        }
        printf ("find %lu kv, hashtable size: %lu, capacity: %lu, loadfactor: %f\n", find,
                hashtable->Size (), hashtable->Capacity (), hashtable->LoadFactor ());
    };
    read_fun ();

    hashtable->MinorReHashAll ();
    read_fun ();

    hashtable->MinorReHashAll ();
    read_fun ();

    return 0;
}