#include "io_report.h"
#include <inttypes.h>
// DEFINE_int32(stats_interval,  1000000,  "Interval that print status");
// DEFINE_int64(report_interval, 0,       "report speed time interval in second");
int64_t report_interval = 0;
int32_t stats_interval = 10000000;

namespace util {

Stats::Stats() { id_ = 0; Start(); }
Stats::Stats(int id) { id_ = id; Start(); }

void Stats::Start() {
    start_ = Env::Default()->NowNanos();
    next_report_time_ = start_ + report_interval * 1000000;
    next_report_ = 100;
    last_op_finish_ = start_;
    last_report_done_ = 0;
    last_report_finish_ = start_;
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    finish_ = start_;
    report_flag_ = 1;
    message_.clear();
}
    
void Stats::Merge(const Stats& other) {
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // Just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
}

void Stats::Stop() {
    finish_ = Env::Default()->NowNanos();
    seconds_ = (finish_ - start_) * 1e-6;
}

void Stats::AddMessage(Slice msg) {
    AppendWithSpace(&message_, msg);
}
void Stats::AppendWithSpace(std::string* str, Slice msg) {
    if (msg.empty()) return;
    if (!str->empty()) {
        str->push_back(' ');
    }
    str->append(msg.data(), msg.size());
}
    
void Stats::PrintSpeed(int64_t done, uint64_t now) {
    int64_t nsecs_since_last = now - last_report_finish_;
    std::string cur_time = Env::Default()->TimeToString(now/1000000000);
    fprintf(stdout,
            "%s ... thread %u: (%" PRIu64 ",%" PRIu64 ") ops and "
            "(%.1f,%.1f) ops/second in (%.6f,%.6f) seconds\n",
            cur_time.c_str(), 
            id_,
            done - last_report_done_, done,
            (done - last_report_done_) /
            (nsecs_since_last / 1000000000.0),
            done / ((now - start_) / 1000000000.0),
            (now - last_report_finish_) / 1000000000.0,
            (now - start_) / 1000000000.0);
    last_report_finish_ = now;
    last_report_done_ = done;
    fflush(stdout);
}

void Stats::FinishedSingleOp() {
    int64_t done = done_.fetch_add(1, std::memory_order_relaxed);
    uint64_t now = Env::Default()->NowNanos();
    if (done >= next_report_) {
        if      (next_report_ < 1000)   next_report_ += 100;
        else if (next_report_ < 5000)   next_report_ += 500;
        else if (next_report_ < 10000)  next_report_ += 1000;
        else if (next_report_ < 50000)  next_report_ += 5000;
        else if (next_report_ < 100000) next_report_ += 10000;
        else if (next_report_ < 500000) next_report_ += 50000;
        else if (next_report_ < 1000000)next_report_ += 100000;
        else                            next_report_ += 500000;
        fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )done, "");
        fflush(stderr);
    }
    if (report_interval > 0) {
        if (now < next_report_time_) return;
        int flag = report_flag_.fetch_sub(1, std::memory_order_relaxed);
        if (flag == 0) {
            next_report_time_ += report_interval * 1000000;
            PrintSpeed(done, now);
            report_flag_ = 1;
        }
    }
    else if(done > 0 && done % stats_interval == 0) {
        PrintSpeed(done, now);
    }

}

void Stats::AddBytes(int64_t n) {
    bytes_.fetch_add(n, std::memory_order_relaxed);
}

    
void Stats::Report(const Slice& name) {
    // Pretend at least one op was done in case we are running a benchmark
    // that does not call FinishedSingleOp().
    if (done_ < 1) done_ = 1;

    std::string extra;
    double elapsed = (finish_ - start_);
    // printf("start time: %.2f - end time: %.2f/ duration: %.2f\n", start_, finish_, elapsed);
    if (bytes_.load() > 0) {
        // Rate is computed on actual elapsed time, not the sum of per-thread
        // elapsed times.
        char rate[100];
        snprintf(rate, sizeof(rate), "%6.1f MB/s",
                (bytes_.load() / 1048576.0) / elapsed * 1e9);
        extra = rate;
    }
    AppendWithSpace(&extra, message_);

    double throughput = (double)done_/elapsed * 1e9;
    fprintf(stdout, "\033[1;34m[%s]: %11.3f nanos/op %ld ops/sec;%s%s\033[0m\n",
            name.ToString().c_str(),
            elapsed / done_,
            (long)throughput,
            (extra.empty() ? "" : " "),
            extra.c_str());
    fflush(stdout);
}


}