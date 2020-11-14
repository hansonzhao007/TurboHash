#include "turbo/spinlock.h"
#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <thread>
#include <algorithm>
#include <atomic>
#include <unistd.h>
#include "util/env.h"
#include "turbo/hash_function.h"
using namespace util;


// static int kThreadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};
std::string print_binary(uint32_t bitmap)
{
    char buffer[1024];
    const char *bit_rep[16] = {
        [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
        [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
        [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
        [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
    };
    sprintf(buffer, "%s%s%s%s%s%s%s%s", 
        bit_rep[(bitmap >> 28) & 0x0F],
        bit_rep[(bitmap >> 24) & 0x0F],
        bit_rep[(bitmap >> 20) & 0x0F],
        bit_rep[(bitmap >> 16) & 0x0F],
        bit_rep[(bitmap >> 12) & 0x0F],
        bit_rep[(bitmap >>  8) & 0x0F],
        bit_rep[(bitmap >>  4) & 0x0F],
        bit_rep[(bitmap >>  0) & 0x0F]
    );
    return buffer;
};

const int kThreadNum = 16;
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
    auto start_time = Env::Default()->NowNanos();
    int kLoops = 10000;
    for (int i = 0; i < kThreadNum; i++) {
        // each worker add 10000,
        workers.push_back(std::thread([kLoops, &lock_value, i]() 
        {   
            // util::Env::Default()->PinCore(kThreadIDs[i]);
            size_t loop = kLoops;
            while (loop--) {
                usleep(1);
                SpinLockScope<0> lock_scope((turbo_bitspinlock*)lock_value);
            }
        }));
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    auto end_time = Env::Default()->NowNanos();
    printf("spinlock Speed: %f Mops/s.\n", (double)(kLoops * kThreadNum) / (end_time - start_time) * 1000.0);

    {
        std::atomic<int> tmp(0);
        std::vector<std::thread> workers2;
        start_time = Env::Default()->NowNanos();
        for (int i = 0; i < kThreadNum; i++) {
            // each worker add 10000,
            workers2.push_back(std::thread([&tmp, i]() 
            {   
                // util::Env::Default()->PinCore(kThreadIDs[i]);
                int loop = 1000000;
                while (loop--) {
                    tmp.fetch_add(1, std::memory_order_relaxed);
                }
            }));
        }
        std::for_each(workers2.begin(), workers2.end(), [](std::thread &t) 
        {
            t.join();
        });
        end_time = Env::Default()->NowNanos();
        printf("fetchadd Speed: %f Mops/s.\n", (double)tmp.load() / (end_time - start_time) * 1000.0);
    }

    {
        std::atomic<int> tmp(0);
        std::vector<std::thread> workers2;
        
        ShardLock locks;
        int kLoops = 100000;
        start_time = Env::Default()->NowNanos();
        for (int i = 0; i < kThreadNum; i++) {
            // each worker add 10000,
            workers2.push_back(std::thread([&locks, i, kLoops]() 
            {   
                // util::Env::Default()->PinCore(kThreadIDs[i]);
                int loop = kLoops;
                while (loop--) {
                    int x = turbo::wyhash32();
                    ShardLockScope lock(&locks, x);
                }
            }));
        }
        std::for_each(workers2.begin(), workers2.end(), [](std::thread &t) 
        {
            t.join();
        });
        end_time = Env::Default()->NowNanos();
        printf("ShardLock Speed: %f Mops/s.\n", (double)(kLoops * kThreadNum) / (end_time - start_time) * 1000.0);
    }

    {
        std::atomic<int> tmp(0);
        std::vector<std::thread> workers2;
        
        ShardSpinLock locks(10);
        int kLoops = 100000;
        start_time = Env::Default()->NowNanos();
        for (int i = 0; i < kThreadNum; i++) {
            // each worker add 10000,
            workers2.push_back(std::thread([&locks, i, kLoops]() 
            {   
                // util::Env::Default()->PinCore(kThreadIDs[i]);
                int loop = kLoops;
                while (loop--) {
                    int x = turbo::wyhash32();
                    locks.Lock(x);
                    locks.Unlock(x);
                }
            }));
        }
        std::for_each(workers2.begin(), workers2.end(), [](std::thread &t) 
        {
            t.join();
        });
        end_time = Env::Default()->NowNanos();
        printf("ShardSpinLock Speed: %f Mops/s.\n", (double)(kLoops * kThreadNum) / (end_time - start_time) * 1000.0);
    }

    {
        std::atomic<int> tmp(0);
        std::vector<std::thread> workers2;
        
        AtomicSpinLock locks;
        int kLoops = 100000;
        start_time = Env::Default()->NowNanos();
        for (int i = 0; i < kThreadNum; i++) {
            // each worker add 10000,
            workers2.push_back(std::thread([&locks, i, kLoops]() 
            {   
                // util::Env::Default()->PinCore(kThreadIDs[i]);
                int loop = kLoops;
                while (loop--) {
                    int x = turbo::wyhash32();
                    locks.lock();
                    locks.unlock();
                }
            }));
        }
        std::for_each(workers2.begin(), workers2.end(), [](std::thread &t) 
        {
            t.join();
        });
        end_time = Env::Default()->NowNanos();
        printf("AtomicSpinLock Speed: %f Mops/s.\n", (double)(kLoops * kThreadNum) / (end_time - start_time) * 1000.0);
    }

    {
        char* tmp_mem = new char [17];
        memset(tmp_mem, 0xFF, 17);
        printf("tmp_mem content: %s\n", print_binary(*(uint32_t*)tmp_mem).c_str());
        tmp_mem[2] = 'a';
        printf("tmp_mem content: %s\n", print_binary(*(uint32_t*)tmp_mem).c_str());

        std::atomic<bool>* spinloc = (std::atomic<bool>*)(tmp_mem);
        printf("spinlock_init val: %d\n", spinloc->load());
        spinloc->store(true);
        printf("spinlock_init val: %d\n", spinloc->load());
        printf("tmp_mem content: %s\n", print_binary(*(uint32_t*)tmp_mem).c_str());
    }
    
}