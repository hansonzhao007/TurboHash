#pragma once

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "random.h"
#include "slice.h"

#define KEY_LEN ((15))

static constexpr uint64_t kRandNumMax = (1LU << 60);
static constexpr uint64_t kRandNumMaxMask = kRandNumMax - 1;

static uint64_t u64Rand (const uint64_t& min, const uint64_t& max) {
    static thread_local std::mt19937 generator (std::random_device{}());
    std::uniform_int_distribution<uint64_t> distribution (min, max);
    return distribution (generator);
}

class RandomKeyTrace {
public:
    RandomKeyTrace (size_t count) {
        count_ = count;
        keys_.resize (count);

        printf ("generate %lu keys\n", count);
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, count),
                           [&](const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   //    uint64_t num = u64Rand (1LU, kRandNumMax);
                                   keys_[i] = i;
                               }
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        printf ("generate duration %f s.\n", duration.count () / 1000000.0);

        Randomize ();
    }

    ~RandomKeyTrace () {}

    void Randomize (void) {
        // printf ("randomize %lu keys\n", keys_.size ());
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, keys_.size ()),
                           [&](const tbb::blocked_range<uint64_t>& range) {
                               auto rng = std::default_random_engine{};
                               std::shuffle (keys_.begin () + range.begin (),
                                             keys_.begin () + range.end (), rng);
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        // printf ("randomize duration %f s.\n", duration.count () / 1000000.0);
    }

    class RangeIterator {
    public:
        RangeIterator (std::vector<size_t>* pkey_vec, size_t start, size_t end)
            : pkey_vec_ (pkey_vec), end_index_ (end), cur_index_ (start) {}

        inline bool Valid () { return (cur_index_ < end_index_); }

        inline size_t operator* () { return (*pkey_vec_)[cur_index_]; }

        inline size_t& Next () { return (*pkey_vec_)[cur_index_++]; }

        std::vector<size_t>* pkey_vec_;
        size_t end_index_;
        size_t cur_index_;
    };

    class Iterator {
    public:
        Iterator (std::vector<size_t>* pkey_vec, size_t start_index, size_t range)
            : pkey_vec_ (pkey_vec),
              range_ (range),
              end_index_ (start_index % range_),
              cur_index_ (start_index % range_),
              begin_ (true) {}

        Iterator () {}

        inline bool Valid () { return (begin_ || cur_index_ != end_index_); }

        inline size_t& Next () {
            begin_ = false;
            size_t index = cur_index_;
            cur_index_++;
            if (cur_index_ >= range_) {
                cur_index_ = 0;
            }
            return (*pkey_vec_)[index];
        }

        inline size_t operator* () { return (*pkey_vec_)[cur_index_]; }

        std::string Info () {
            char buffer[128];
            sprintf (buffer, "valid: %s, cur i: %lu, end_i: %lu, range: %lu",
                     Valid () ? "true" : "false", cur_index_, end_index_, range_);
            return buffer;
        }

        std::vector<size_t>* pkey_vec_;
        size_t range_;
        size_t end_index_;
        size_t cur_index_;
        bool begin_;
    };

    Iterator trace_at (size_t start_index, size_t range) {
        return Iterator (&keys_, start_index, range);
    }

    RangeIterator Begin (void) { return RangeIterator (&keys_, 0, keys_.size ()); }

    RangeIterator iterate_between (size_t start, size_t end) {
        return RangeIterator (&keys_, start, end);
    }

    size_t count_;
    std::vector<size_t> keys_;
};

class RandomKeyTraceString {
public:
    RandomKeyTraceString (size_t count) : count_ (count) {
        static const char alphabet[] =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789";

        const size_t N_STRS = count;
        static const size_t S_LEN = KEY_LEN;

        keys_.resize (N_STRS);

        printf ("generate %lu keys\n", N_STRS);
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, N_STRS),
                           [&](const tbb::blocked_range<uint64_t>& range) {
                               for (uint64_t i = range.begin (); i != range.end (); i++) {
                                   keys_[i].reserve (S_LEN);
                                   std::generate_n (std::back_inserter (keys_[i]), S_LEN,
                                                    [&]() { return alphabet[u64Rand (0, 61)]; });
                               }
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        printf ("generate duration %f s.\n", duration.count () / 1000000.0);
        keys_non_.resize (N_STRS);
        for (size_t i = 0; i < N_STRS; i++) {
            keys_non_[i] = keys_[i];
            keys_non_[i][0] = '_';
            keys_non_[i][KEY_LEN / 2] = '_';
        }
    }

    ~RandomKeyTraceString () {}

    void Randomize (void) {
        // printf ("randomize %lu keys\n", keys_.size ());
        auto starttime = std::chrono::system_clock::now ();
        tbb::parallel_for (tbb::blocked_range<uint64_t> (0, keys_.size ()),
                           [&](const tbb::blocked_range<uint64_t>& range) {
                               auto rng = std::default_random_engine{};
                               std::shuffle (keys_.begin () + range.begin (),
                                             keys_.begin () + range.end (), rng);
                           });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::system_clock::now () - starttime);
        // printf ("randomize duration %f s.\n", duration.count () / 1000000.0);
    }

    class RangeIterator {
    public:
        RangeIterator (std::vector<std::string>* pkey_vec, size_t start, size_t end)
            : pkey_vec_ (pkey_vec), end_index_ (end), cur_index_ (start) {}

        inline bool Valid () { return (cur_index_ < end_index_); }

        inline std::string& Next () { return (*pkey_vec_)[cur_index_++]; }

        std::vector<std::string>* pkey_vec_;
        size_t end_index_;
        size_t cur_index_;
    };

    class Iterator {
    public:
        Iterator (std::vector<std::string>* pkey_vec, size_t start_index, size_t range)
            : pkey_vec_ (pkey_vec),
              range_ (range),
              end_index_ (start_index % range_),
              cur_index_ (start_index % range_),
              begin_ (true) {}

        Iterator () {}

        inline bool Valid () { return (begin_ || cur_index_ != end_index_); }

        inline std::string& Next () {
            begin_ = false;
            size_t index = cur_index_;
            cur_index_++;
            if (cur_index_ >= range_) {
                cur_index_ = 0;
            }
            return (*pkey_vec_)[index];
        }

        inline std::string& operator* () { return (*pkey_vec_)[cur_index_]; }

        std::string Info () {
            char buffer[128];
            sprintf (buffer, "valid: %s, cur i: %lu, end_i: %lu, range: %lu",
                     Valid () ? "true" : "false", cur_index_, end_index_, range_);
            return buffer;
        }

        std::vector<std::string>* pkey_vec_;
        size_t range_;
        size_t end_index_;
        size_t cur_index_;
        bool begin_;
    };

    Iterator trace_at (size_t start_index, size_t range) {
        return Iterator (&keys_, start_index, range);
    }

    Iterator nontrace_at (size_t start_index, size_t range) {
        return Iterator (&keys_non_, start_index, range);
    }

    RangeIterator Begin (void) { return RangeIterator (&keys_, 0, keys_.size ()); }

    RangeIterator iterate_between (size_t start, size_t end) {
        return RangeIterator (&keys_, start, end);
    }

    size_t count_;
    std::vector<std::string> keys_;
    std::vector<std::string> keys_non_;
};

enum YCSBOpType { kYCSB_Write, kYCSB_Read, kYCSB_Query, kYCSB_ReadModifyWrite };

inline uint32_t wyhash32 () {
    static thread_local uint32_t wyhash32_x = random ();
    wyhash32_x += 0x60bee2bee120fc15;
    uint64_t tmp;
    tmp = (uint64_t)wyhash32_x * 0xa3b195354a39b70d;
    uint32_t m1 = (tmp >> 32) ^ tmp;
    tmp = (uint64_t)m1 * 0x1b03738712fad5c9;
    uint32_t m2 = (tmp >> 32) ^ tmp;
    return m2;
}

class YCSBGenerator {
public:
    // Generate
    YCSBGenerator () {}

    inline YCSBOpType NextA () {
        // ycsba: 50% reads, 50% writes
        uint32_t rnd_num = wyhash32 ();

        if ((rnd_num & 0x1) == 0) {
            return kYCSB_Read;
        } else {
            return kYCSB_Write;
        }
    }

    inline YCSBOpType NextB () {
        // ycsbb: 95% reads, 5% writes
        // 51/1024 = 0.0498
        uint32_t rnd_num = wyhash32 ();

        if ((rnd_num & 1023) < 51) {
            return kYCSB_Write;
        } else {
            return kYCSB_Read;
        }
    }

    inline YCSBOpType NextC () { return kYCSB_Read; }

    inline YCSBOpType NextD () {
        // ycsbd: read latest inserted records
        return kYCSB_Read;
    }

    inline YCSBOpType NextF () {
        // ycsba: 50% reads, 50% writes
        uint32_t rnd_num = wyhash32 ();

        if ((rnd_num & 0x1) == 0) {
            return kYCSB_Read;
        } else {
            return kYCSB_ReadModifyWrite;
        }
    }
};

// Helper for quickly generating random data.
class RandomGenerator {
private:
    std::string data_;
    int pos_;

public:
    RandomGenerator () : data_ (""), pos_ (0) {
        // We use a limited amount of data over and over again and ensure
        // that it is larger than the compression window (32KB), and also
        // large enough to serve all typical value sizes we want to write.
        util::Random rnd (301);
        std::string piece;
        while (data_.size () < 1048576) {
            // Add a short fragment that is as compressible as specified
            // by FLAGS_compression_ratio.
            RandomString (&rnd, 100, &piece);
            data_.append (piece);
        }
        pos_ = 0;
    }

    Slice RandomString (util::Random* rnd, int len, std::string* dst) {
        dst->resize (len);
        for (int i = 0; i < len; i++) {
            (*dst)[i] = static_cast<char> (' ' + rnd->Uniform (95));  // ' ' .. '~'
        }
        return Slice (*dst);
    }

    Slice Generate (size_t len) {
        if (pos_ + len > data_.size ()) {
            pos_ = 0;
            assert (len < data_.size ());
        }
        pos_ += len;
        return Slice (data_.data () + pos_ - len, len);
    }
};
