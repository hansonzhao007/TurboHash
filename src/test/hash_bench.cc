#include <immintrin.h>
#include <cstdlib>
#include <unordered_map>
#include <libcuckoo/cuckoohash_map.hh>

#include "util/env.h"
#include "lightning/hash_function.h"
#include "lightning/hash_table.h"
#include "util/robin_hood.h"
#include "util/io_report.h"
#include "util/trace.h"
#include "util/perf_util.h"

#include "absl/container/flat_hash_map.h"

#include "test_util.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;

DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_bool(print_thread_read, false, "");
DEFINE_int32(thread_read, 1, "");
DEFINE_int32(thread_write, 1, "");
DEFINE_int32(associate_size, 16, "");
DEFINE_int32(bucket_size, 128 << 10, "bucket count");
DEFINE_int32(probe_type, 0, "\
    0: probe within bucket, \
    1: probe within cell");
DEFINE_int32(cell_type, 0, "\
    0: 128 byte cell, \
    1: 256 byte cell");
DEFINE_bool(locate_cell_with_h1, false, "using partial hash h1 to locate cell inside bucket or not");

static int kThreadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};

class HashBench {
public:
    HashBench(size_t bucket_count, size_t assocaite_count, size_t cell_type):
        max_count_(bucket_count * assocaite_count * (cell_type == 0 ? 14 : 28)),
        key_trace_(max_count_) {
        value_ = "v";
        arm_sig_int();
        signal(SIGALRM, &sigalrm_handler);  // set a signal handler
    }

    void PrintSpeed(std::string name, double loadfactor, size_t size, size_t duration_ns, bool read) {
        printf("%33s - Load Factor: %.2f. %s   %10lu key, Speed: %5.2f Mops/s. Time: %12lu ns\n", 
            name.c_str(), 
            loadfactor,
            read ? "Read  " : "Insert",
            size,
            (double)size / duration_ns * 1000.0, 
            duration_ns
            );
    }

    size_t LTHashSpeedTest() {
        size_t inserted_num = 0;
        util::Stats stats;
        lthash::HashTable* hashtable = HashTableCreate(FLAGS_cell_type, FLAGS_probe_type, FLAGS_bucket_size, FLAGS_associate_size);
        uint64_t i = 0;
        bool res = true;
        auto key_iterator = key_trace_.trace_at(0, max_count_);
        hashtable->WarmUp();
        debug_perf_switch();
        auto time_start = Env::Default()->NowNanos();
        while (res && i < max_count_) {
            res = hashtable->Put(key_iterator.Next(), value_);
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "inserting%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        printf("Total put: %lu\n", i - 1);
        std::string name = "lthash:" + hashtable->ProbeStrategyName();
        PrintSpeed(name, hashtable->LoadFactor(), hashtable->Size(), time_end - time_start, false);
        inserted_num = i - 1;
        
        std::vector<std::thread> workers(FLAGS_thread_read);
        std::vector<size_t> counts(FLAGS_thread_read, 0);
        kRunning = true;
        if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
        debug_perf_switch();
        time_start = Env::Default()->NowNanos();
        for (int t = 0; t < FLAGS_thread_read; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                Env::PinCore(kThreadIDs[t]);
                std::string value;
                bool res = true;
                size_t i = 0;
                size_t start_offset = random() % inserted_num;
                if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                while (kRunning && key_iterator.Valid() && res) {
                    res = hashtable->Get(key_iterator.Next(), &value);
                    if ((i++ & 0xFFFFF) == 0) {
                        fprintf(stderr, "thread: %2d reading%*s-%03d->\r", t,  int(i >> 20), " ", int(i >> 20));fflush(stderr);
                    }
                }
                if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, res ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                counts[t] = i ;
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        time_end = Env::Default()->NowNanos();
        debug_perf_stop();
        size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
        PrintSpeed(name, hashtable->LoadFactor(), read_count, time_end - time_start, true);
        if (FLAGS_print_thread_read)
        for(int i = 0; i < FLAGS_thread_read; i++) {
            printf("thread %2d read: %10lu\n", i, counts[i]);
        }

        delete hashtable;
        return inserted_num;
    }

        
    lthash::HashTable* HashTableCreate(int cell_type, int probe_type, int bucket, int associate) {
        if (0 == cell_type && 0 == probe_type)
            return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
        if (0 == cell_type && 1 == probe_type)
            return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinCell>(bucket, associate, FLAGS_locate_cell_with_h1);
        if (1 == cell_type && 0 == probe_type)
            return new lthash::DramHashTable<lthash::CellMeta256, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
        if (1 == cell_type && 1 == probe_type)
            return new lthash::DramHashTable<lthash::CellMeta256, lthash::ProbeWithinCell>(bucket, associate, FLAGS_locate_cell_with_h1);
        else
            return new lthash::DramHashTable<lthash::CellMeta128, lthash::ProbeWithinBucket>(bucket, associate, FLAGS_locate_cell_with_h1);
    }
        
    template <class HashMap, class ValueType>
    void HashSpeedTest(const std::string& name, size_t inserted_num) {
        HashMap map;
        std::string key = "ltkey";
        uint64_t i = 0;
        bool res = true;
        map.reserve(inserted_num);
        auto key_iterator = key_trace_.trace_at(0, inserted_num);
        auto time_start = Env::Default()->NowNanos();
        while (key_iterator.Valid()) {
            map.insert({key_iterator.Next(), value_});
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "inserting%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        PrintSpeed(name.c_str(), map.load_factor(), map.size(), time_end - time_start, false);

        std::vector<std::thread> workers(FLAGS_thread_read);
        std::vector<size_t> counts(FLAGS_thread_read, 0);
        kRunning = true;
        if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
        time_start = Env::Default()->NowNanos();
        for (int t = 0; t < FLAGS_thread_read; t++) {
            
            workers[t] = std::thread([&, t]
            {
                // core function
                Env::PinCore(kThreadIDs[t]);
                size_t i = 0;
                auto iter = map.begin();
                size_t start_offset = random() % inserted_num;
                if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                while (kRunning && key_iterator.Valid() && iter != map.end()) {
                    iter = map.find(key_iterator.Next());
                    if ((i++ & 0xFFFFF) == 0) {
                        fprintf(stderr, "thread: %2d reading%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                    }
                }
                if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, iter != map.end() ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                counts[t] = i ;
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        time_end = Env::Default()->NowNanos();
        size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
        PrintSpeed(name.c_str(), map.load_factor(), read_count, time_end - time_start, true);
        if (FLAGS_print_thread_read)
        for(int i = 0; i < FLAGS_thread_read; i++) {
            printf("thread %2d read: %10lu\n", i, counts[i]);
        }
    } 


    void CuckooSpeedTest(const std::string& name, size_t inserted_num) {
        libcuckoo::cuckoohash_map<std::string, std::string> map;
        uint64_t i = 0;
        bool res = true;
        map.reserve(inserted_num);
        auto key_iterator = key_trace_.trace_at(0, inserted_num);
        auto time_start = Env::Default()->NowNanos();
        while (key_iterator.Valid()) {
            map.insert(key_iterator.Next(), value_);
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "inserting%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        PrintSpeed(name.c_str(), map.load_factor(), i, time_end - time_start, false);

        std::vector<std::thread> workers(FLAGS_thread_read);
        std::vector<size_t> counts(FLAGS_thread_read, 0);
        kRunning = true;
        if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
        time_start = Env::Default()->NowNanos();
        for (int t = 0; t < FLAGS_thread_read; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                Env::PinCore(kThreadIDs[t]);
                size_t i = 0;
                std::string out;
                bool is_find = false;
                size_t start_offset = random() % inserted_num;
                if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                do {
                    is_find = map.find(key_iterator.Next(), out);
                    if ((i++ & 0xFFFFF) == 0) {
                        fprintf(stderr, "thread: %2d reading%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                    }
                } while (kRunning && key_iterator.Valid() && is_find);
                if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s, running: %s\n", t, is_find ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                counts[t] = i ;
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        time_end = Env::Default()->NowNanos();
        size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
        PrintSpeed(name.c_str(), map.load_factor(), read_count, time_end - time_start, true);
    
        if (FLAGS_print_thread_read)
        for(int i = 0; i < FLAGS_thread_read; i++) {
            printf("thread %2d read: %10lu\n", i, counts[i]);
        }
    }

    

    static void sig_int(int sig)
    {
        printf("Exiting on signal %d\r", sig);
        kRunning = false;
    }

    static void arm_sig_int(void)
    {
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = sig_int;
        act.sa_flags = SA_RESTART;
        sigaction(SIGINT, &act, NULL);
    }

    static void sigalrm_handler(int sig)
    {
        printf("Exiting on timeout %d\r", sig);
        kRunning = false;
    }

private:
    size_t max_count_;
    std::string value_;
    RandomKeyTrace key_trace_;
    static bool kRunning;
};
bool HashBench::kRunning = true;

int main(int argc, char *argv[]) {
    Env::PinCore(kThreadIDs[15]);
    debug_perf_ppid();
    ParseCommandLineFlags(&argc, &argv, true);

    HashBench hash_bench(FLAGS_bucket_size, FLAGS_associate_size, FLAGS_cell_type);
    
    size_t inserted_num = hash_bench.LTHashSpeedTest();
    // inserted_num = hash_bench.LTHashSpeedTest();
    hash_bench.HashSpeedTest<robin_hood::unordered_map<std::string, std::string>, std::string >("robin_hood::unordered_map", inserted_num);
    
    hash_bench.HashSpeedTest<absl::flat_hash_map<std::string, std::string>, std::string >("absl::flat_hash_map", inserted_num);
    // HashSpeedTest<std::unordered_map<std::string, std::string>, std::string >("std::unordered_map", inserted_num);
    hash_bench.CuckooSpeedTest("CuckooHashMap", inserted_num);
    return 0;
}

