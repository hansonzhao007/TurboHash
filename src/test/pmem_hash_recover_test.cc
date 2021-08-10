#include "gflags/gflags.h"
#include "turbo/turbo_hash_pmem.h"
#include "util/logger.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

// Step 1. run "./test_pmem_hash_recover_test --op=write"
// Step 2. run "./test_pmem_hash_recover_test --op=read"
// Step 3. check the log file *.log
DEFINE_string (op, "write", "write, read, delete");
int main (int argc, char* argv[]) {
    ParseCommandLineFlags (&argc, &argv, true);
    using HashTable = turbo_pmem::unordered_map<std::string, std::string>;

    if (FLAGS_op == "write") {
        const size_t COUNT = 100000;
        auto* hashtable = new HashTable;

        hashtable->Initialize (8, 128);

        printf ("------- Iterate empty hash table ------\n");
        hashtable->IterateAll ();

        auto thread_info = hashtable->getThreadInfo ();

        bool succ = true;
        size_t find = 0;
        for (size_t i = 0; i < COUNT && succ; i++) {
            succ = hashtable->Put ("key" + std::to_string (i), "value" + std::to_string (i),
                                   thread_info);
            if ((succ)) find++;
        }
        printf ("inserted %lu kv\n", find);

        find = 0;
        for (size_t i = 0; i < 100 && succ; i++) {
            succ = hashtable->Put ("key" + std::to_string (i), "updat" + std::to_string (i),
                                   thread_info);
            if ((succ)) find++;
        }
        printf ("update %lu kv\n", find);

        auto read_fun = [&hashtable] {
            size_t find = 0;
            std::string value_buffer;
            auto read_callback = [&] (HashTable::RecordType record) {
                value_buffer = record.value ();
            };
            auto tinfo = hashtable->getThreadInfo ();
            for (size_t i = 0; i < COUNT; i++) {
                std::string key = "key" + std::to_string (i);
                auto res = hashtable->Find (key, tinfo, read_callback);
                if (i < 10 || (i & 0x7FF) == 0) {
                    INFO ("Get key: %s. val: %s\n", key.c_str (), value_buffer.c_str ());
                }
                if ((res)) find++;
            }
            printf ("find %lu key\n", find);
        };
        read_fun ();
    } else {
        const size_t COUNT = 100000;
        auto* hashtable = new turbo_pmem::unordered_map<std::string, std::string>;
        hashtable->Recover ();

        auto read_fun = [&hashtable] {
            size_t find = 0;
            std::string value_buffer;
            auto read_callback = [&] (HashTable::RecordType record) {
                value_buffer = record.value ();
            };
            auto tinfo = hashtable->getThreadInfo ();
            for (size_t i = 0; i < COUNT; i++) {
                std::string key = "key" + std::to_string (i);
                auto res = hashtable->Find (key, tinfo, read_callback);
                if (i < 10 || (i & 0x7FF) == 0) {
                    INFO ("Recover Get key: %s. value: %s\n", key.c_str (), value_buffer.c_str ());
                }
                if ((res)) find++;
            }
            printf ("find %lu key\n", find);
        };
        read_fun ();
    }

    return 0;
}