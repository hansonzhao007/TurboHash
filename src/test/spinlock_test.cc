#include "turbo/spinlock.h"
#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <thread>
#include <algorithm>
#include <atomic>
#include <unistd.h>
#include "util/env.h"
using namespace util;


static int kThreadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};
const int kThreadNum = 15;
int main() {
    std::atomic<int> aint(0);
    unsigned* lock_value = (unsigned*)malloc(4);
    for (int i = 0; i < 32; ++i) {
        printf("test and set bit: %d\n", i);
        printf("\tlock_value before: %09x\n", *lock_value);
        char test_set_res = turbo_atomic_bittestandset_x86(lock_value, i);    
        printf("\tlock_value  after: %09x. test&set result: %d\n", *lock_value, test_set_res);
        test_set_res = turbo_atomic_bittestandset_x86(lock_value, i);    
        printf("\tlock_value  after: %09x. test&set result: %d (retry)\n", *lock_value, test_set_res);
        char test_reset_res = turbo_atomic_bittestandreset_x86(lock_value, i);
        printf("\tlock_value  after: %09x. test&reset result: %d (reset)\n", *lock_value, test_reset_res);
        test_reset_res = turbo_atomic_bittestandreset_x86(lock_value, i);
        printf("\tlock_value  after: %09x. test&reset result: %d (retry)\n", *lock_value, test_reset_res);
    }

    *lock_value = 0xFCFCDAC8;
    turbo_bit_spin_lock(lock_value, 0);
    printf("succ lock. lock value: %08x\n", *lock_value);

    turbo_bit_spin_unlock(lock_value, 0);
    printf("succ unlock. lock value: %08x\n", *lock_value);

    *lock_value = 0;
    std::vector<std::thread> workers;
    size_t shared_num = 0;
    auto start_time = Env::Default()->NowNanos();
    for (int i = 0; i < kThreadNum; i++) {
        // each worker add 10000,
        workers.push_back(std::thread([&shared_num, &lock_value, i]() 
        {   
            // util::Env::Default()->PinCore(kThreadIDs[i]);
            size_t loop = 100000;
            while (loop--) {
                usleep(1);
                SpinLockScope lock_scope((turbo_bitspinlock*)lock_value);
                // critical section
                shared_num++;
                // turbo_bit_spin_unlock(lock_value, 0);
            }
        }));
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    auto end_time = Env::Default()->NowNanos();
    printf("spinlock Speed: %f Mops/s. Add result: %lu\n", 
        (double)shared_num / (end_time - start_time) * 1000.0,
        shared_num);

    {
        std::atomic<int> tmp(0);
        std::vector<std::thread> workers;
        auto start_time = Env::Default()->NowNanos();
        for (int i = 0; i < kThreadNum; i++) {
            // each worker add 10000,
            workers.push_back(std::thread([&shared_num, &tmp, i]() 
            {   
                util::Env::Default()->PinCore(kThreadIDs[i]);
                int loop = 1000000;
                while (loop--) {
                    tmp.fetch_add(1, std::memory_order_relaxed);
                }
            }));
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        auto end_time = Env::Default()->NowNanos();
        printf("fetchadd Speed: %f Mops/s. Add result: %d\n", 
            (double)tmp.load() / (end_time - start_time) * 1000.0,
            tmp.load());
    }
}