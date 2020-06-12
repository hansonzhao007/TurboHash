#include <immintrin.h>
#include <cstdlib>
#include <unordered_map>
#include <libcuckoo/cuckoohash_map.hh>

#include "util/env.h"

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

DEFINE_bool(print_thread_read, false, "");
DEFINE_int32(thread_read, 2, "");
DEFINE_int32(thread_write, 1, "");
DEFINE_int32(associate_size, 16, "");
DEFINE_int32(bucket_size, 256 << 10, "bucket count");
DEFINE_int32(probe_type, 0, "\
    0: probe within bucket, \
    1: probe within cell");
DEFINE_int32(cell_type, 0, "\
    0: 128 byte cell, \
    1: 256 byte cell");
DEFINE_bool(locate_cell_with_h1, false, "using partial hash h1 to locate cell inside bucket or not");

static int kThreadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};

size_t MAX_RANGE = 100000000;
std::vector<std::string> kInsertedKeys;
std::vector<std::string> kSearchKeys;

template <class HashMap, class ValueType>
void HashSpeedTest(const std::string& name, size_t inserted_num);
void CuckooSpeedTest(const std::string& name, size_t inserted_num);
size_t LTHashSpeedTest(bool generate_search_key);
lthash::HashTable* HashTableCreate(int cell_type, int probe_type, int bucket, int associate);


bool kRunning = true;
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

void sigalrm_handler(int sig)
{
    printf("Exiting on timeout %d\r", sig);
	kRunning = false;
}


int main(int argc, char *argv[]) {
    Env::PinCore(kThreadIDs[15]);
    debug_perf_ppid();
    arm_sig_int();
    signal(SIGALRM, &sigalrm_handler);  // set a signal handler

    ParseCommandLineFlags(&argc, &argv, true);
    MAX_RANGE = FLAGS_bucket_size * FLAGS_associate_size * (FLAGS_cell_type == 0 ? 14 : 28);
    kInsertedKeys = GenerateAllKeysInRange(0, MAX_RANGE);

    size_t inserted_num = LTHashSpeedTest(true);
    // inserted_num = LTHashSpeedTest(false);
    HashSpeedTest<robin_hood::unordered_map<std::string, std::string>, std::string >("robin_hood::unordered_map", inserted_num);
    HashSpeedTest<absl::flat_hash_map<std::string, std::string>, std::string >("absl::flat_hash_map", inserted_num);
    // HashSpeedTest<std::unordered_map<std::string, std::string>, std::string >("std::unordered_map", inserted_num);
    CuckooSpeedTest("CuckooHashMap", inserted_num);
    return 0;
}

size_t LTHashSpeedTest(bool generate_search_key) {
    size_t inserted_num = 0;
    util::Stats stats;
    lthash::HashTable* hashtable = HashTableCreate(FLAGS_cell_type, FLAGS_probe_type, FLAGS_bucket_size, FLAGS_associate_size);
    uint64_t i = 0;
    bool res = true;
    debug_perf_switch();
    auto time_start = Env::Default()->NowNanos();
    while (res && i < MAX_RANGE) {
        res = hashtable->Put(kInsertedKeys[i++], "v");
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    auto time_end = Env::Default()->NowNanos();
    debug_perf_switch();
    printf("lthash(%25s) - Load Factor: %.2f. Insert   %10lu key, Speed: %5.2f Mops/s. Time: %lu ns. Total put: %10lu\n", 
        hashtable->ProbeStrategyName().c_str(), 
        hashtable->LoadFactor(), 
        hashtable->Size(), 
        (double)hashtable->Size() / (time_end - time_start) * 1000.0, 
        (time_end - time_start),
        i - 1);
    inserted_num = i - 1;
    

    std::vector<std::thread> workers(FLAGS_thread_read);
    std::vector<size_t> counts(FLAGS_thread_read, 0);
    if (generate_search_key) kSearchKeys =  GenerateRandomKeys(0, inserted_num, inserted_num, i * 123 + 123);

    kRunning = true;
    alarm(6);  // set an alarm for 6 seconds from now
    debug_perf_switch();
    time_start = Env::Default()->NowNanos();
    for (int t = 0; t < FLAGS_thread_read; t++) {
        workers[t] = std::thread([t, inserted_num, &hashtable, &counts]
        {
            // core function
            Env::PinCore(kThreadIDs[t]);
            std::string value;
            bool res = true;
            size_t i = 0;
            size_t start_offset = random() % inserted_num;
            // printf("thread %2d start offet: %8lu\n", t, start_offset);
            while (kRunning && i < inserted_num && res) {
                res = hashtable->Get(kSearchKeys[(start_offset + i++) % inserted_num], &value);
                if ((i & 0xFFFFF) == 0) {
                    fprintf(stderr, "thread: %2d%*s-%03d->\r", t,  int(i >> 20), " ", int(i >> 20));fflush(stderr);
                }
            }
            counts[t] = i - 1;
        });
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    time_end = Env::Default()->NowNanos();
    debug_perf_stop();
    size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
    printf("lthash(%25s) - Load Factor: %.2f. Read     %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", 
        hashtable->ProbeStrategyName().c_str(), 
        hashtable->LoadFactor(), 
        read_count, 
        (double)(read_count) / (time_end - time_start) * 1000.0, 
        (time_end - time_start));
    if (FLAGS_print_thread_read)
    for(int i = 0; i < FLAGS_thread_read; i++) {
        printf("thread %2d read: %10lu\n", i, counts[i]);
    }

    delete hashtable;
    return inserted_num;
}

template <class HashMap, class ValueType>
void HashSpeedTest(const std::string& name, size_t inserted_num) {
    HashMap map;
    ValueType value = "v";
    std::string key = "ltkey";
    uint64_t i = 0;
    bool res = true;
    map.reserve(inserted_num);
    auto time_start = Env::Default()->NowNanos();
    while (i < inserted_num) {
        map.insert({kInsertedKeys[i++], value});
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    auto time_end = Env::Default()->NowNanos();
    printf("%33s - Load Factor: %.2f. Insert   %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", 
        name.c_str(), 
        map.load_factor(), 
        map.size(), 
        (double)map.size() / (time_end - time_start) * 1000.0, 
        (time_end - time_start)
        );

    std::vector<std::thread> workers(FLAGS_thread_read);
    std::vector<size_t> counts(FLAGS_thread_read, 0);
    kRunning = true;
    alarm(6);  // set an alarm for 6 seconds from now
    time_start = Env::Default()->NowNanos();
    for (int t = 0; t < FLAGS_thread_read; t++) {
        workers[t] = std::thread([t, inserted_num, &map, &counts]
        {
            // core function
            Env::PinCore(kThreadIDs[t]);
            size_t i = 0;
            auto iter = map.find(kSearchKeys[0]);
            size_t start_offset = random() % inserted_num;
            // printf("thread %2d start offet: %8lu\n", t, start_offset);
            while (kRunning && iter != map.end() && i < inserted_num) {
                iter = map.find(kSearchKeys[(start_offset + i++) % inserted_num]);
                if ((i & 0xFFFFF) == 0) {
                    fprintf(stderr, "thread: %2d%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                }
            }
            counts[t] = i - 1;
        });
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    time_end = Env::Default()->NowNanos();
    size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
    printf("%33s - Load Factor: %.2f. Read     %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", 
        name.c_str(),
        map.load_factor(), 
        read_count, 
        (double)(read_count) / (time_end - time_start) * 1000.0, 
        (time_end - time_start));
    if (FLAGS_print_thread_read)
    for(int i = 0; i < FLAGS_thread_read; i++) {
        printf("thread %2d read: %10lu\n", i, counts[i]);
    }
} 

void CuckooSpeedTest(const std::string& name, size_t inserted_num) {
    libcuckoo::cuckoohash_map<std::string, std::string> map;
    std::string value = "v";
    std::string key = "ltkey";
    
    uint64_t i = 0;
    bool res = true;
    map.reserve(inserted_num);
    auto time_start = Env::Default()->NowNanos();
    while (i < inserted_num) {
        map.insert(kInsertedKeys[i++], value);
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    auto time_end = Env::Default()->NowNanos();
    printf("%33s - Load Factor: %.2f. Insert   %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", 
        name.c_str(), 
        map.load_factor(), 
        map.size(), 
        (double)map.size() / (time_end - time_start) * 1000.0, 
        (time_end - time_start)
        );

    auto search_keys = GenerateRandomKeys(0, inserted_num, inserted_num);

    std::vector<std::thread> workers(FLAGS_thread_read);
    std::vector<size_t> counts(FLAGS_thread_read, 0);
    kRunning = true;
    alarm(6);  // set an alarm for 6 seconds from now
    time_start = Env::Default()->NowNanos();
    for (int t = 0; t < FLAGS_thread_read; t++) {
        workers[t] = std::thread([t, inserted_num, &map, &counts]
        {
            // core function
            Env::PinCore(kThreadIDs[t]);
            size_t i = 0;
            std::string out;
            bool is_find = false;
            size_t start_offset = random() % inserted_num;
            // printf("thread %2d start offet: %8lu\n", t, start_offset);
            do {
                is_find = map.find(kSearchKeys[(start_offset + i++) % inserted_num], out);
                if ((i & 0xFFFFF) == 0) {
                    fprintf(stderr, "thread: %2d%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                }
            } while (kRunning && i < inserted_num && is_find);
            counts[t] = i - 1;
        });
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    time_end = Env::Default()->NowNanos();
    size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
    printf("%33s - Load Factor: %.2f. Read     %10lu key, Speed: %5.2f Mops/s. Time: %lu ns\n", 
            name.c_str(), 
            map.load_factor(), 
            read_count, 
            (double)(read_count) / (time_end - time_start) * 1000.0, 
            (time_end - time_start)
        );
    if (FLAGS_print_thread_read)
    for(int i = 0; i < FLAGS_thread_read; i++) {
        printf("thread %2d read: %10lu\n", i, counts[i]);
    }
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
