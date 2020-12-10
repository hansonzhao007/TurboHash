#include "util/trace.h"
#include "util/io_report.h"

#include "test_util.h"
const uint64_t KEY_COUNT = 80000000;
using namespace util;
int main() {
    char str[128] = "1234567890";
    std::string test_str(str+1, str+4);
    printf("test string construction with two addrees: %s\n", test_str.c_str());
    Stats stats;
    util::TraceUniform* trace = new util::TraceUniform(123);
    uint64_t num = 0;
    uint64_t time_start, time_end;
    double duration;
    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        num = trace->Next();
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "iteration %*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("num final: %lu\n", num);
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("TraceUniform speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);


    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        num = random();
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "iteration %*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("num final: %lu\n", num);
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("Random       speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);
    

    RandomString rnd(trace);
    std::string res;
    time_start = Env::Default()->NowMicros();
    for (uint64_t i = 0; i < KEY_COUNT; i++) {
        res = rnd.next();
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "iteration %*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("string final: %s\n", res.data());
    time_end = Env::Default()->NowMicros();
    duration = (time_end - time_start);
    printf("RandomString speed (%lu record): %f Mops/s\n", KEY_COUNT, KEY_COUNT / duration);

    {
        RandomKeyTrace keytrace(KEY_COUNT);
        RandomKeyTrace::Iterator key_iterator = keytrace.trace_at(0, KEY_COUNT);
        time_start = Env::Default()->NowMicros();
        size_t i;
        
        for (i = 0; key_iterator.Valid(); i++) {
            const std::string& k = key_iterator.Next();
            if ((i & 0xFFFFF) == 0) {
                fprintf(stderr, "iteration %*s-%03d->. key: %s\r", int(i >> 20), " ", int(i >> 20), k.c_str());fflush(stderr);
            }
        }
        time_end = Env::Default()->NowMicros();
        printf("string final: %s\n", key_iterator.Next().c_str());
        duration = (time_end - time_start);
        printf("RandomKeyTrace speed (%lu record): %f Mops/s\n", i, i / duration);
    }

    {
        RandomKeyTrace2 keytrace(KEY_COUNT);
        RandomKeyTrace2::Iterator key_iterator = keytrace.trace_at(0, KEY_COUNT);
        time_start = Env::Default()->NowMicros();
        size_t i;
        
        for (i = 0; key_iterator.Valid(); i++) {
            key_iterator.Next();
            if ((i & 0xFFFFF) == 0) {
                fprintf(stderr, "iteration %*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        time_end = Env::Default()->NowMicros();
        printf("string final: %s\n", key_iterator.Next().c_str());
        duration = (time_end - time_start);
        printf("RandomKeyTrace2 speed (%lu record): %f Mops/s\n", i, i / duration);
    }

    return 0;
}