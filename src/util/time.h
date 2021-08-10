#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

namespace util {

// Returns the number of micro-seconds since some fixed point in time. Only
// useful for computing deltas of time.
inline uint64_t NowMicros () {
    static constexpr uint64_t kUsecondsPerSecond = 1000000;
    struct ::timeval tv;
    ::gettimeofday (&tv, nullptr);
    return static_cast<uint64_t> (tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
}
// Returns the number of nano-seconds since some fixed point in time. Only
// useful for computing deltas of time in one run.
// Default implementation simply relies on NowMicros.
// In platform-specific implementations, NowNanos() should return time points
// that are MONOTONIC.
inline uint64_t NowNanos () {
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t> (ts.tv_sec) * 1000000000L + ts.tv_nsec;
}

std::string Execute (const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype (&pclose)> pipe (popen (cmd.c_str (), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error ("popen() failed!");
    }
    while (fgets (buffer.data (), buffer.size (), pipe.get ()) != nullptr) {
        result += buffer.data ();
    }
    return result;
}

}  // namespace util
