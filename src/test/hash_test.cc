#include "turbo/turbo_hash.h"


#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


int main() {
    turbo::unordered_map<std::string, std::string> map;

    const size_t COUNT = 100000;
    auto* hashtable = new turbo::unordered_map<std::string, std::string> (8, 8192);
    printf("------- Iterate empty hash table ------\n");
    hashtable->IterateAll();

    bool succ = true;
    size_t find = 0;
    for (size_t i = 0 ; i < COUNT && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "value" + std::to_string(i));
        if ((succ)) find++;
    }
    printf("inserted %lu kv, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());

    auto read_fun = [&hashtable] {
        bool succ = true;
        std::string value;
        size_t find = 0;
        for (size_t i = 0; i < COUNT && succ; i++) {
            succ = hashtable->Get("key" + std::to_string(i), &value);
            if ((succ)) find++;
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
    hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    read_fun();

    hashtable->MinorReHashAll();
    hashtable->DebugInfo();
    read_fun();

    printf("size of atomic bool: %lu\n", sizeof(std::atomic<bool>));
    

    return 0;
}