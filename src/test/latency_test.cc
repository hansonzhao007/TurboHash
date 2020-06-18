#include "util/pmm_util.h"
#include "util/env.h"
#include "test_util.h"
#include <immintrin.h>


#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

DEFINE_int32(loop, 2, "");
DEFINE_bool(random, false, "");

using namespace util;

int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    util::Env::PinCore(0);
    int    ACCESS_SIZE_BIT = 6;
    size_t TRACE_LEN = 1LU << 28;
    size_t MEM_MASK  = (TRACE_LEN - 1) & 0xFFFFFFFFFFFFFFC0;
    size_t MEM_LEN = TRACE_LEN << ACCESS_SIZE_BIT;
    char* addr = (char*)aligned_alloc(1 << ACCESS_SIZE_BIT, MEM_LEN);
    
    memset(addr, 0, MEM_LEN);
    __m128i loadbuf;
    if (!FLAGS_random)
    {
        printf("-------- sequential access -------\n");
        auto start_time = util::Env::Default()->NowMicros();
        int loop = FLAGS_loop;
        while (loop--) { 
            for (size_t i = 0; i < TRACE_LEN; ++i) {
                Load256(addr + (i << ACCESS_SIZE_BIT));
            }
            printf("finish one round\n");
        } 
        
        auto end_time   = util::Env::Default()->NowMicros();
        auto duration = end_time - start_time;
        printf("sequential speed: %f Mops/s, duration: %lu\n", (double)TRACE_LEN * FLAGS_loop / duration, duration);
    } else 
    {
        printf("-------- random access -------\n");
        auto start_time = util::Env::Default()->NowMicros();
        int loop = FLAGS_loop;
        while(loop--)
        {
            size_t count = 0;
            for (size_t i = 0; count < TRACE_LEN;) {
                Load256(addr + (i << ACCESS_SIZE_BIT));
                count++;
                i & 3 == 1 ? i += 101 : i+= 107;
                i >= TRACE_LEN ? i -= TRACE_LEN : i;
            }
            printf("finish one round\n");
        }
        
        auto end_time   = util::Env::Default()->NowMicros();
        auto duration = end_time - start_time;
        printf("random speed: %f Mops/s, duration: %lu \n", (double)TRACE_LEN * FLAGS_loop / duration, duration);
    }

    return 0;
}

