#include "util/trace.h"
#include "util/io_report.h"

#include "test_util.h"
const uint64_t KEY_COUNT = 80000000;;
using namespace util;
int main() {
    char str[128] = "1234567890";
    std::string test_str(str+1, str+4);
    printf("test string construction with two addrees: %s\n", test_str.c_str());
    Stats stats;
    util::TraceUniform* trace = new util::TraceUniform(123);
    // for (int i = 0; i < 10000; i++) {
    //     printf("randome: %f\n", trace->RandomDouble());
    // }
    uint64_t num = 0;
    uint64_t time_start, time_end;
    double duration;
    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        num = trace->Next();
    }
    printf("num final: %lu\n", num);
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("TraceUniform speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);


    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        num = random();
    }
    printf("num final: %lu\n", num);
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("Random       speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);
    

    RandomString rnd(trace);
    util::Slice res;
    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        res = rnd.next();
    }
    printf("string final: %s\n", res.data());
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("RandomString speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);


    RandomKeyTrace keytrace(KEY_COUNT);
    RandomKeyTrace::Iterator key_iterator = keytrace.trace_at(0, KEY_COUNT / 2);
    time_start = Env::Default()->NowMicros();
    size_t i;
    for (i = 0; key_iterator.Valid(); i++) {
        res = key_iterator.Next();
    }
    printf("string final: %s\n", res.data());
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("RandomString speed (%lu record): %f Mops/s\n", i, i / duration);
    return 0;
}