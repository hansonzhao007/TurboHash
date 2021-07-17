#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include "tbb/tbb.h"
#include "turbo/turbo_hash.h"

using namespace turbo;

uint64_t u64Rand (const uint64_t& min, const uint64_t& max) {
    static thread_local std::mt19937 generator (std::random_device{}());
    std::uniform_int_distribution<uint64_t> distribution (min, max);
    return distribution (generator);
}

void multithreaded (char** argv) {
    std::cout << "multi threaded:" << std::endl;

    uint64_t n = std::atoll (argv[1]);
    uint64_t* keys = new uint64_t[n];

    // Generate keys
    for (uint64_t i = 0; i < n; i++)
        // dense, sorted
        keys[i] = i + 1;
    if (atoi (argv[2]) == 1)
        // dense, random
        std::random_shuffle (keys, keys + n);
    if (atoi (argv[2]) == 2) {
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, n),
                           [&] (const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   keys[i] = u64Rand (1LU, (1LU << 63) - 1);
                               }
                           });
    }

    printf ("operation,n,ops/s\n");
    turbo::unordered_map<size_t, size_t> tree;

    // Build tree
    {
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, n),
                           [&] (const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   tree.Put (keys[i], keys[i]);
                               }
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        printf ("insert,%ld,%f\n", n, (n * 1.0) / duration.count ());
    }

    {
        // Lookup
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, n),
                           [&] (const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   auto val = tree.Find (keys[i]);
                                   if (val->second () != keys[i]) {
                                       std::cout << "wrong key read: " << val
                                                 << " expected:" << keys[i] << std::endl;
                                       throw;
                                   }
                               }
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        printf ("lookup,%ld,%f\n", n, (n * 1.0) / duration.count ());
    }

    {
        auto starttime = std::chrono::system_clock::now ();

        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, n),
                           [&] (const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   tree.Delete (keys[i]);
                               }
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        printf ("remove,%ld,%f\n", n, (n * 1.0) / duration.count ());
    }
    delete[] keys;
}

int main (int argc, char** argv) {
    if (argc != 3) {
        printf (
            "usage: %s n 0|1|2\nn: number of keys\n0: sorted keys\n1: dense keys\n2: sparse keys\n",
            argv[0]);
        return 1;
    }

    multithreaded (argv);

    return 0;
}