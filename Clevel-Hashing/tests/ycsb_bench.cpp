#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include <iterator>
#include <thread>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <time.h>
#include <sys/time.h>
#include <thread>               // std::thread
#include <mutex>                // std::mutex
#include <condition_variable>   // std::condition_variable
 
#include "../../examples/libpmemobj_cpp_examples_common.hpp"
#include "../polymorphic_string.h"
#include "../profile.hpp"

#include <libpmemobj++/experimental/clevel_hash.hpp>
#include <libpmemobj++/experimental/level_hash.hpp>
#include <libpmemobj++/experimental/cceh.hpp>
#include <libpmemobj++/experimental/clht.hpp>

#include "test_util.h"
#include "histogram.h"
#include <gflags/gflags.h>
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

DEFINE_string(filepath, "/mnt/pmem/objpool.data", "");
DEFINE_uint32(batch, 1000000, "report batch");
DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_uint32(thread, 1, "");
DEFINE_uint64(report_interval, 0, "Report interval in seconds");
DEFINE_uint64(stats_interval, 10000000, "Report interval in ops");
DEFINE_uint64(value_size, 8, "The value size");
DEFINE_uint64(num, 1 * 1000000LU, "Number of total record");
DEFINE_uint64(read,  1 * 1000000, "Number of read operations");
DEFINE_uint64(write, 1 * 1000000, "Number of read operations");
DEFINE_bool(hist, false, "");
DEFINE_string(benchmarks, "load,readrandom", "");

// #define TYPE_CCEH
#define TYPE_CLEVEL
// #define TYPE_LEVEL
// #define TYPE_CLHT

#define DEBUG_RESIZING 1

#define MICRO_BENCH
// #define MACRO_BENCH

#ifdef MICRO_BENCH
// Initialize around 10 million slots
// 16 M
#define CCEH_INITIAL_DEPTH 14U 
// 12 million
#define CLEVEL_HASH_POWER 20
// 12 million
#define LEVEL_HASH_POWER 21
// 12 million
#define CLHT_N_BUCKETS ((1LU << 22))
#else 
// Initialize around 100 million slots
// 8 M
#define CCEH_INITIAL_DEPTH 13U 
// 12 million
#define CLEVEL_HASH_POWER 20
// 12 million
#define LEVEL_HASH_POWER 21

#define CLHT_N_BUCKETS 16384
#endif

using string_t = polymorphic_string;
namespace nvobj = pmem::obj;

class key_equal {
public:
	template <typename M, typename U>
	bool operator()(const M &lhs, const U &rhs) const
	{
		return lhs == rhs;
	}
};
class string_hasher {
	/* hash multiplier used by fibonacci hashing */
	static const size_t hash_multiplier = 11400714819323198485ULL;

public:
	using transparent_key_equal = key_equal;

	size_t operator()(const polymorphic_string &str) const
	{
		return hash(str.c_str(), str.size());
	}


private:
	size_t hash(const char *str, size_t size) const
	{
		size_t h = 0;
		for (size_t i = 0; i < size; ++i) {
			h = static_cast<size_t>(str[i]) ^ (h * hash_multiplier);
		}
		return h;
	}
};

namespace {

#define KEY_LEN 15
#define VALUE_LEN 15

#ifdef TYPE_CCEH
#define LAYOUT "CCEH"
typedef nvobj::experimental::CCEH
	persistent_map_type;
#elif defined TYPE_CLEVEL
#define LAYOUT "CLEVEL"
typedef nvobj::experimental::clevel_hash<string_t, string_t, string_hasher,
	std::equal_to<string_t>, CLEVEL_HASH_POWER>
	persistent_map_type;
#elif defined TYPE_LEVEL
#define LAYOUT "LEVEL"
typedef nvobj::experimental::level_hash<string_t, string_t, string_hasher,
	std::equal_to<string_t>>
	persistent_map_type;
#elif defined TYPE_CLHT
#define LAYOUT "CLHT"
typedef nvobj::experimental::clht<string_t, string_t, string_hasher,
	std::equal_to<string_t>>
	persistent_map_type;
#endif

typedef persistent_map_type::ret Ret;
typedef persistent_map_type::key_type Key;
typedef persistent_map_type::mapped_type T;
typedef persistent_map_type::value_type ValueType;

struct root {
	nvobj::persistent_ptr<persistent_map_type> cons;
};

class Stats {
public:
    int tid_;
    double start_;
    double finish_;
    double seconds_;
    double next_report_time_;
    double last_op_finish_;
    unsigned last_level_compaction_num_;
    util::HistogramImpl hist_;

    uint64_t done_;
    uint64_t last_report_done_;
    uint64_t last_report_finish_;
    uint64_t next_report_;
    std::string message_;

    Stats() {Start(); }
    explicit Stats(int id) :
        tid_(id){ Start(); }
    
    void Start() {
        start_ = NowMicros();
        next_report_time_ = start_ + FLAGS_report_interval * 1000000;
        next_report_ = 100;
        last_op_finish_ = start_;
        last_report_done_ = 0;
        last_report_finish_ = start_;
        last_level_compaction_num_ = 0;
        done_ = 0;
        seconds_ = 0;
        finish_ = start_;
        message_.clear();
        hist_.Clear();
    }

    void Merge(const Stats& other) {
        hist_.Merge(other.hist_);
        done_ += other.done_;
        seconds_ += other.seconds_;
        if (other.start_ < start_) start_ = other.start_;
        if (other.finish_ > finish_) finish_ = other.finish_;

        // Just keep the messages from one thread
        if (message_.empty()) message_ = other.message_;
    }
    
    void Stop() {
        finish_ = NowMicros();
        seconds_ = (finish_ - start_) * 1e-6;;
    }

    void StartSingleOp() {
        last_op_finish_ = NowMicros();
    }

    void PrintSpeed() {
        uint64_t now = NowMicros();
        int64_t usecs_since_last = now - last_report_finish_;

        std::string cur_time = TimeToString(now/1000000);
        printf( "%s ... thread %d: (%lu,%lu) ops and "
                "( %.1f,%.1f ) ops/second in (%.4f,%.4f) seconds\n",
                cur_time.c_str(), 
                tid_,
                done_ - last_report_done_, done_,
                (done_ - last_report_done_) /
                (usecs_since_last / 1000000.0),
                done_ / ((now - start_) / 1000000.0),
                (now - last_report_finish_) / 1000000.0,
                (now - start_) / 1000000.0);       
        last_report_finish_ = now;
        last_report_done_ = done_;
        fflush(stdout);
    }
    
    static void AppendWithSpace(std::string* str, const std::string& msg) {
        if (msg.empty()) return;
        if (!str->empty()) {
            str->push_back(' ');
        }
        str->append(msg.data(), msg.size());
    }

    void AddMessage(const std::string& msg) {
        AppendWithSpace(&message_, msg);
    }
    
    inline void FinishedBatchOp(size_t batch) {
        double now = NowNanos();
        last_op_finish_ = now;
        done_ += batch;
        if (unlikely(done_ >= next_report_)) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )done_, "");
            
            if (FLAGS_report_interval == 0 && (done_ % FLAGS_stats_interval) == 0) {
                PrintSpeed(); 
                return;
            } 
            fflush(stderr);
            fflush(stdout);
        }

        if (FLAGS_report_interval != 0 && NowNanos()> next_report_time_) {
            next_report_time_ += FLAGS_report_interval * 1000000;
            PrintSpeed(); 
        }
    }

    inline void FinishedSingleOp() {
        double now = NowNanos();
        last_op_finish_ = now;

        done_++;
        if (done_ >= next_report_) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )done_, "");
            
            if (FLAGS_report_interval == 0 && (done_ % FLAGS_stats_interval) == 0) {
                PrintSpeed(); 
                return;
            } 
            fflush(stderr);
            fflush(stdout);
        }

        if (FLAGS_report_interval != 0 && NowNanos()> next_report_time_) {
            next_report_time_ += FLAGS_report_interval * 1000000;
            PrintSpeed(); 
        }
    }

    std::string TimeToString(uint64_t secondsSince1970) {
        const time_t seconds = (time_t)secondsSince1970;
        struct tm t;
        int maxsize = 64;
        std::string dummy;
        dummy.reserve(maxsize);
        dummy.resize(maxsize);
        char* p = &dummy[0];
        localtime_r(&seconds, &t);
        snprintf(p, maxsize,
                "%04d/%02d/%02d-%02d:%02d:%02d ",
                t.tm_year + 1900,
                t.tm_mon + 1,
                t.tm_mday,
                t.tm_hour,
                t.tm_min,
                t.tm_sec);
        return dummy;
    }

    void Report(const Slice& name, bool print_hist = false) {
        // Pretend at least one op was done in case we are running a benchmark
        // that does not call FinishedSingleOp().
        if (done_ < 1) done_ = 1;

        std::string extra;

        AppendWithSpace(&extra, message_);

        double elapsed = (finish_ - start_) * 1e-6;

        double throughput = (double)done_/elapsed;
        
        printf( "%-12s : %11.3f micros/op %lf Mops/s;%s%s\n",
                name.ToString().c_str(),
                elapsed * 1e6 / done_,
                throughput/1024/1024,
                (extra.empty() ? "" : " "),
                extra.c_str());
        if (print_hist) {
            fprintf(stdout, "Nanoseconds per op:\n%s\n", hist_.ToString().c_str());
        }

        fflush(stdout);
        fflush(stderr);
    }
};


// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  std::mutex mu;
  std::condition_variable cv;
  int total;

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized;
  int num_done;
  bool start;

  SharedState(int total):
    total(total), num_initialized(0), num_done(0), start(false) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
    int tid;             // 0..n-1 when running in n threads
    // Random rand;         // Has different seeds for different threads
    Stats stats;
    SharedState* shared;
    ThreadState(int index) : 
        tid(index),
        stats(index) {
    }
};


class Duration {
public:
    Duration(uint64_t max_seconds, int64_t max_ops, int64_t ops_per_stage = 0) {
        max_seconds_ = max_seconds;
        max_ops_= max_ops;
        ops_per_stage_ = (ops_per_stage > 0) ? ops_per_stage : max_ops;
        ops_ = 0;
        start_at_ = NowMicros();
    }

    inline int64_t GetStage() { return std::min(ops_, max_ops_ - 1) / ops_per_stage_; }

    inline bool Done(int64_t increment) {
        if (increment <= 0) increment = 1;    // avoid Done(0) and infinite loops
        ops_ += increment;

        if (max_seconds_) {
        // Recheck every appx 1000 ops (exact iff increment is factor of 1000)
        auto granularity = 1000;
        if ((ops_ / granularity) != ((ops_ - increment) / granularity)) {
            uint64_t now = NowMicros();
            return ((now - start_at_) / 1000000) >= max_seconds_;
        } else {
            return false;
        }
        } else {
        return ops_ > max_ops_;
        }
    }

    inline int64_t Ops() {
        return ops_;
    }
 private:
    uint64_t max_seconds_;
    int64_t max_ops_;
    int64_t ops_per_stage_;
    int64_t ops_;
    uint64_t start_at_;
};


#if defined(__linux)
static std::string TrimSpace(std::string s) {
    size_t start = 0;
    while (start < s.size() && isspace(s[start])) {
        start++;
    }
    size_t limit = s.size();
    while (limit > start && isspace(s[limit-1])) {
        limit--;
    }
    return std::string(s.data() + start, limit - start);
}
#endif

}

class Benchmark {

public:
    uint64_t num_;
    int value_size_;
    size_t reads_;
    size_t writes_;
    RandomKeyTrace* key_trace_;
    size_t trace_size_;
    nvobj::pool<root> pop_;
    decltype(pop_.root()->cons) map_;
    Benchmark():
        num_(FLAGS_num),
        value_size_(FLAGS_value_size),
        reads_(FLAGS_read),
        writes_(FLAGS_write),
        key_trace_(nullptr) {
        
        remove(FLAGS_filepath.c_str()); // delete the mapped file.
        pop_ = nvobj::pool<root>::create(FLAGS_filepath.c_str(), LAYOUT, PMEMOBJ_MIN_POOL * 15360, S_IWUSR | S_IRUSR);
        auto proot = pop_.root();
        {
            nvobj::transaction::manual tx(pop_);

            #ifdef TYPE_CCEH
		    proot->cons = nvobj::make_persistent<persistent_map_type>(CCEH_INITIAL_DEPTH);
            #elif defined TYPE_CLEVEL
            proot->cons = nvobj::make_persistent<persistent_map_type>();
		    proot->cons->set_thread_num(FLAGS_thread);
            #elif defined TYPE_LEVEL
            proot->cons = nvobj::make_persistent<persistent_map_type>((uint64_t)LEVEL_HASH_POWER, 1);
            #elif defined TYPE_CLHT
            proot->cons = nvobj::make_persistent<persistent_map_type>((uint64_t)CLHT_N_BUCKETS);
            #endif

		    nvobj::transaction::commit();
        }

        map_ = pop_.root()->cons;
	    printf("Initialization done.\n");
        printf("Initial capacity %ld\n", map_->capacity());

    }

    ~Benchmark() {
        
    }

    void Run() {
        trace_size_ = FLAGS_num;
        printf("key trace size: %lu\n", trace_size_);
        key_trace_ = new RandomKeyTrace(trace_size_);
        if (reads_ == 0) {
            reads_ = key_trace_->count_;
            FLAGS_read = key_trace_->count_;
        }
        PrintHeader();
        bool fresh_db = true;
        // run benchmark
        bool print_hist = false;
        const char* benchmarks = FLAGS_benchmarks.c_str();        
        while (benchmarks != nullptr) {
            int thread = FLAGS_thread;
            void (Benchmark::*method)(ThreadState*) = nullptr;
            const char* sep = strchr(benchmarks, ',');
            std::string name;
            if (sep == nullptr) {
                name = benchmarks;
                benchmarks = nullptr;
            } else {
                name = std::string(benchmarks, sep - benchmarks);
                benchmarks = sep + 1;
            }
            if (name == "load") {
                fresh_db = true;
                method = &Benchmark::DoWrite;                
            } if (name == "loadlat") {
                fresh_db = true;
                print_hist = true;
                method = &Benchmark::DoWriteLat;                
            } else if (name == "allloadfactor") {
                fresh_db = true;
                method = &Benchmark::DoLoadFactor;                
            } else if (name == "overwrite") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoOverWrite;                
            } else if (name == "readrandom") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoRead;                
            } else if (name == "readnon") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoReadNon;                
            } else if (name == "readlat") {
                fresh_db = false;
                print_hist = true;
                key_trace_->Randomize();
                method = &Benchmark::DoReadLat;                
            } else if (name == "readnonlat") {
                fresh_db = false;
                print_hist = true;
                key_trace_->Randomize();
                method = &Benchmark::DoReadNonLat;                
            } else if (name == "stats") {
                fresh_db = false;
                thread = 1;
                method = &Benchmark::DoStats;
            }

            IPMWatcher watcher(name);
            if (method != nullptr) RunBenchmark(thread, name, method, print_hist);
        }
    }

    inline Ret Find(const string_t& key, size_t tid) {
        #ifdef TYPE_CCEH
        return map_->get((uint8_t*)key.c_str(), key.size());
        #elif defined TYPE_CLEVEL
        return map_->search(key);
        #elif defined TYPE_LEVEL
        return map_->query(key, tid);
        #elif defined TYPE_CLHT
        return map_->get(key);
        #endif
    }

    inline Ret Insert(const string_t& key, const string_t& val, size_t tid) {
        #ifdef TYPE_CCEH
        return map_->insert((uint8_t*)key.c_str(), (uint8_t*)val.c_str(), KEY_LEN, VALUE_LEN, tid);
        #elif defined TYPE_CLEVEL
        return map_->insert(ValueType(key, val), tid + 1, tid +1);
        #elif defined TYPE_LEVEL
        return map_->insert(ValueType(key, val), tid);
        #elif defined TYPE_CLHT
        return map_->put(ValueType(key, val), tid);
        #endif
    }

    void DoStats(ThreadState* thread) {   
        // thread->stats.Start(); 
        // double load_factor = hashtable_->LoadFactor();
        // char buf[100];
        // snprintf(buf, sizeof(buf), "load factor: %f", load_factor);
        // thread->stats.AddMessage(buf);
    }

    void DoRead(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoRead lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();        
        while (!duration.Done(batch) && key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {    
                size_t ikey = key_iterator.Next();  
                char key[KEY_LEN] = {0};
                snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
                auto ret = Find({key, KEY_LEN}, thread->tid);
                if (!ret.found) {
                    not_find++;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadNon(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoReadNon lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();        
        while (!duration.Done(batch) && key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {      
                size_t ikey = key_iterator.Next() + num_;  
                char key[KEY_LEN] = {0};
                snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
                auto ret = Find({key, KEY_LEN}, thread->tid);
                if (!ret.found) {
                    not_find++;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadLat(ThreadState* thread) {
        if (key_trace_ == nullptr) {
            perror("DoReadLat lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();
        while (!duration.Done(1) && key_iterator.Valid()) {   
            size_t ikey = key_iterator.Next();  
            char key[KEY_LEN] = {0};
            snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);

            auto time_start = NowNanos();
            auto ret = Find({key, KEY_LEN}, thread->tid);
            if (!ret.found) {
                not_find++;
            }
            auto time_duration = NowNanos() - time_start;

            thread->stats.hist_.Add(time_duration); 
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadNonLat(ThreadState* thread) {
        if (key_trace_ == nullptr) {
            perror("DoReadLat lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();
        while (!duration.Done(1) && key_iterator.Valid()) {        
            size_t ikey = key_iterator.Next() + num_;  /* Generate a key out of the insertion range */
            char key[KEY_LEN] = {0};
            snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);

            auto time_start = NowNanos();
            auto ret = Find({key, KEY_LEN}, thread->tid);
            if (!ret.found) {
                not_find++;
            }
            auto time_duration = NowNanos() - time_start;
            
            thread->stats.hist_.Add(time_duration); 
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoWrite(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoWrite lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        thread->stats.Start();
        std::string val(value_size_, 'v');
        while (key_iterator.Valid()) {
            for (uint64_t j = 0; j < batch && key_iterator.Valid(); j++) { 
                size_t ikey = key_iterator.Next();  
                char key[KEY_LEN] = {0};
	            char value[VALUE_LEN] = {0};
                snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
                snprintf(reinterpret_cast<char *>(value), VALUE_LEN, "%lu", ikey);
                auto res = Insert({key, KEY_LEN}, {value, VALUE_LEN}, thread->tid);                
            }
            thread->stats.FinishedBatchOp(batch);
        }
        write_end:
        return;
    }

    void DoWriteLat(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoWriteLat lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        thread->stats.Start();
        while (key_iterator.Valid()) {            
            size_t ikey = key_iterator.Next();  
            char key[KEY_LEN] = {0};
            char value[VALUE_LEN] = {0};
            snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
            snprintf(reinterpret_cast<char *>(value), VALUE_LEN, "%lu", ikey);

            auto time_start = NowNanos();
            auto res = Insert({key, KEY_LEN}, {value, VALUE_LEN}, thread->tid);                
            auto time_duration = NowNanos() - time_start;
            thread->stats.hist_.Add(time_duration);
        }
        write_end:
        return;
    }

    void DoLoadFactor(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoLoadFactor lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        size_t inserted = 0;
        size_t not_inserted = 0;
        thread->stats.Start();      
        while (key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {   
                size_t ikey = key_iterator.Next();  
                char key[KEY_LEN] = {0};
	            char value[VALUE_LEN] = {0};
                snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
                snprintf(reinterpret_cast<char *>(value), VALUE_LEN, "%lu", ikey);
                auto res = Insert({key, KEY_LEN}, {value, VALUE_LEN}, thread->tid);
                inserted++;
            }
            thread->stats.FinishedBatchOp(j);
            printf("Load factor: %.3f\n", (double)(inserted) / map_->capacity());
        }
        write_end:
        return;
    }

    void DoOverWrite(ThreadState* thread) {
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            perror("DoOverWrite lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        Duration duration(FLAGS_readtime, writes_);
        size_t not_inserted = 0;
        thread->stats.Start();
        while (key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {   
                size_t ikey = key_iterator.Next();  
                char key[KEY_LEN] = {0};
	            char value[VALUE_LEN] = {0};
                snprintf(reinterpret_cast<char *>(key),   KEY_LEN,   "%lu", ikey);
                snprintf(reinterpret_cast<char *>(value), VALUE_LEN, "%lu", ikey);
                auto res = Insert({key, KEY_LEN}, {value, VALUE_LEN}, thread->tid);
                if (!res.found) {
                    // success insertion
                } else {
                    // insertion fail
                    not_inserted++;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        write_end:
        printf("Thread %2d: num: %lu, not insert: %lu\n", thread->stats.tid_, interval, not_inserted);
        return;
    }

private:
    struct ThreadArg {
        Benchmark* bm;
        SharedState* shared;
        ThreadState* thread;
        void (Benchmark::*method)(ThreadState*);
    };

    static void ThreadBody(void* v) {
        ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
        SharedState* shared = arg->shared;
        ThreadState* thread = arg->thread;
        {
            std::unique_lock<std::mutex> lck(shared->mu);
            shared->num_initialized++;
            if (shared->num_initialized >= shared->total) {
                shared->cv.notify_all();
            }
            while (!shared->start) {
                shared->cv.wait(lck);
            }
        }

        thread->stats.Start();
        (arg->bm->*(arg->method))(thread);
        thread->stats.Stop();

        {
            std::unique_lock<std::mutex> lck(shared->mu);
            shared->num_done++;
            if (shared->num_done >= shared->total) {
                shared->cv.notify_all();
            }
        }
    }

    void RunBenchmark(int thread_num, const std::string& name, 
                      void (Benchmark::*method)(ThreadState*), bool print_hist) {
        SharedState shared(thread_num);
        ThreadArg* arg = new ThreadArg[thread_num];
        std::thread server_threads[thread_num];
        for (int i = 0; i < thread_num; i++) {
            arg[i].bm = this;
            arg[i].method = method;
            arg[i].shared = &shared;
            arg[i].thread = new ThreadState(i);
            arg[i].thread->shared = &shared;
            server_threads[i] = std::thread(ThreadBody, &arg[i]);
        }

        std::unique_lock<std::mutex> lck(shared.mu);
        while (shared.num_initialized < thread_num) {
            shared.cv.wait(lck);
        }

        shared.start = true;
        shared.cv.notify_all();
        while (shared.num_done < thread_num) {
            shared.cv.wait(lck);
        }

        for (int i = 1; i < thread_num; i++) {
            arg[0].thread->stats.Merge(arg[i].thread->stats);
        }
        arg[0].thread->stats.Report(name, print_hist);
        
        for (auto& th : server_threads) th.join();
    }


    void PrintEnvironment() {
        #if defined(__linux)
        time_t now = time(nullptr);
        fprintf(stderr, "Date:                  %s", ctime(&now));  // ctime() adds newline

        FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo != nullptr) {
            char line[1000];
            int num_cpus = 0;
            std::string cpu_type;
            std::string cache_size;
            while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
                const char* sep = strchr(line, ':');
                if (sep == nullptr) {
                continue;
                }
                std::string key = TrimSpace(std::string(line, sep - 1 - line));
                std::string val = TrimSpace(std::string(sep + 1));
                if (key == "model name") {
                ++num_cpus;
                cpu_type = val;
                } else if (key == "cache size") {
                cache_size = val;
                }
            }
        fclose(cpuinfo);
        fprintf(stderr, "CPU:                   %d * %s\n", num_cpus, cpu_type.c_str());
        fprintf(stderr, "CPUCache:              %s\n", cache_size.c_str());                  
        }
        #endif
    }

    void PrintHeader() {
        fprintf(stdout, "------------------------------------------------\n");                   
        PrintEnvironment();
        fprintf(stdout, "HashType:              %s\n", LAYOUT);
        fprintf(stdout, "Init Capacity:         %lu\n", map_->capacity());
        fprintf(stdout, "Key Size:              %lu\n", KEY_LEN);
        fprintf(stdout, "Val Size:              %lu\n", VALUE_LEN);
        fprintf(stdout, "Entries:               %lu\n", (uint64_t)num_);
        fprintf(stdout, "Trace size:            %lu\n", (uint64_t)trace_size_);                      
        fprintf(stdout, "Read:                  %lu \n", (uint64_t)FLAGS_read);
        fprintf(stdout, "Write:                 %lu \n", (uint64_t)FLAGS_write);
        fprintf(stdout, "Thread:                %lu \n", (uint64_t)FLAGS_thread);
        fprintf(stdout, "Report interval:       %lu s\n", (uint64_t)FLAGS_report_interval);
        fprintf(stdout, "Stats interval:        %lu records\n", (uint64_t)FLAGS_stats_interval);
        fprintf(stdout, "benchmarks:            %s\n", FLAGS_benchmarks.c_str());
        fprintf(stdout, "------------------------------------------------\n");
    }
};

int main(int argc, char *argv[])
{
    ParseCommandLineFlags(&argc, &argv, true);
    
	int sds_write_value = 0;
	pmemobj_ctl_set(NULL, "sds.at_create", &sds_write_value);

    Benchmark benchmark;
    benchmark.Run();
    return 0;
}
